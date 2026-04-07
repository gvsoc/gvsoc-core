#include <cstdio>
#include <queue>
#include <algorithm>
#include <fstream>
#include <functional>

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/mapping_tree.hpp>
#include <vp/fault_injector.hpp>

// we use simple FNV hash
#define HASH_SIZE 	8

// parametrize it 
#define ADDR_BITS	32

using StuckAtMap = std::map<uint64_t, std::vector<vp::FIRequest *>>;

// for priority queue (where should it go?)
struct __fir_CompareTs {
	bool operator()(const vp::FIRequest *a, const vp::FIRequest *b) const
	{
		return a->target_cycle > b->target_cycle;
	}
};

class FIC : public vp::Component, vp::FIC_registrator
{
	
public:
	FIC(vp::ComponentConf &config);

	void register_memory(vp::Component *comp, uint8_t *memory_ptr, uint64_t memory_size, 
		StuckAtMap& stuck_at_map);

	void register_regfile(vp::Component *comp, int type, void *regfile, 
		uint64_t nb_regs, uint64_t reg_size);

	void register_interco(vp::Component *comp, vp::MappingTree *mapping_tree);

	void register_prefetcher(vp::Component *comp, uint8_t *prefetcher_buffer, 
		uint64_t prefetcher_size);

	void register_cache(vp::Component *comp, uint8_t *lines, uint64_t nb_lines,
		uint64_t line_size_bits, uint64_t nb_sets_bits, uint64_t struct_size, 
		uint64_t tag_off, uint64_t dirty_off, uint64_t data_off);

	void reset(bool active) override;
	void stop() override;

	static uint64_t hash_data(uint8_t *start, uint64_t size);
	static vp::FIRequest *fir_deep_copy(vp::FIRequest *req);

private:
	vp::FIRequest *assemble_request(int cmd, int kind, int target_type, int target, 
		uint64_t addr, uint8_t bit, uint64_t duration, uint8_t is_high, uint64_t delay, 
		uint64_t size, uint64_t id);

	// timer callback method
	static void handle_event(vp::Block *__this, vp::ClockEvent *event);

	void inject_fault(vp::FIRequest *fir);
	void enqueue_fault_dynamic(vp::FIRequest *fir);

	// wire for sending faults into global address space
	vp::IoMaster global_as;

	/* I think this should be generalized? Or no? */

	// MEMORIES
	std::vector<uint8_t *> 		memory_ptrs;
	std::vector<uint64_t> 		memory_sizes;
	std::vector<std::string>	memory_paths;
	std::vector<std::reference_wrapper<StuckAtMap>> memory_stuck_at_maps;

	std::queue<vp::FIRequest *> hash_requests;

	// REGFILES
	std::vector<void *> 		regfile_ptrs;
	std::vector<uint64_t> 		regfile_nbs;
	std::vector<uint64_t> 		regfile_sizes;
	std::vector<int> 			regfile_types;
	std::vector<std::string>	regfile_paths;

	// INTERCOS
	std::vector<std::vector<vp::MappingTreeEntry *>> interco_links;
	std::vector<std::string> interco_paths;

	// PREFETCHERS
	std::vector<uint8_t *>		prefetcher_ptrs;
	std::vector<uint64_t>		prefetcher_sizes;
	std::vector<std::string>	prefetcher_paths;

	// CACHES
	std::vector<uint8_t *>		caches_lines;
	std::vector<uint64_t>		caches_nb_lines;
	std::vector<uint64_t>		caches_line_sizes;
	std::vector<uint64_t>		caches_tag_bitwidths;
	// Dont know the stuct, so save relative offsets of fields
	std::vector<uint64_t>		caches_struct_sizes;
	std::vector<uint64_t>		caches_tag_off;
	std::vector<uint64_t>		caches_dirty_off;
	std::vector<uint64_t>		caches_data_off;

	std::vector<std::string>	caches_paths;

	// FAULTS
	std::priority_queue<vp::FIRequest *, std::vector<vp::FIRequest *>, __fir_CompareTs> fault_queue;

	vp::ClockEvent *event;

	uint64_t curr_memory_id 	= 0;
	uint64_t curr_regfile_id 	= 0;
	uint64_t curr_interco_id	= 0;
	uint64_t curr_prefetcher_id	= 0;
	uint64_t curr_cache_id		= 0;

	bool static_setup_done 		= false;

	bool dump_final_cycle_count = false;
	bool dump_memories_data 	= false;
	bool dump_regfiles_data 	= false;
	bool dump_interco_data		= false;
	bool dump_prefetchers_data	= false;
	bool dump_caches_data 		= false;
};

FIC::FIC(vp::ComponentConf &config)
	: vp::Component(config)
{
	new_service("FIC", static_cast<vp::FIC_registrator *>(this));

	this->new_master_port("global_as", &this->global_as);

	this->event = new vp::ClockEvent(this, this, &FIC::handle_event);

	std::string faults_path = get_js_config()->get("faults_path")->get_str();
	if (faults_path != "")
	{
		std::ifstream file(faults_path.c_str());
		if (file.is_open()) 
		{
			long long int cmd, kind, target, target_type, addr, bit;
			long long int duration, is_high, delay, size, id;
			/*
			 *	{CMD}         {KIND} {TARGET TYPE} {TARGET} {ADDR} {BIT} {DURATION} {L|H} {DELAY} {SIZE} {ID}
			 *	inject=0	             mem=0                                       L=0
			 *	hash=1			         reg=1                                       H=1
			 *	mem_data=2
			 *	cycle_count=3
			 *	reg_data=4
			 *  ...
			 */
			while (file >> cmd >> kind >> target_type >> target >> addr 
				>> bit >> duration >> is_high >> delay >> size >> id) 
			{
				vp::FIRequest *fir = this->assemble_request((int) cmd, (int) kind, (int) target_type, 
					(int) target, (uint64_t) addr, (uint8_t ) bit, (uint64_t) duration, 
					(uint8_t) is_high, (uint64_t) delay, (uint64_t) size, (uint64_t) id);

				if (fir->cmd == vp::FICommand::INJECT)
				{
					fir->target_cycle = fir->delay;	
					this->fault_queue.push(fir);
				}
				else if (fir->cmd == vp::FICommand::HASH)
				{
					this->hash_requests.push(fir);
				}
				else if (fir->cmd == vp::FICommand::GET_CYCLES)
  				{
					/* This command being here makes sense only 
					 * if the user wants the final cycle count */
					this->dump_final_cycle_count = true;
					delete fir;
				}
				else if (fir->cmd == vp::FICommand::GET_MEM_DEV_DATA)
				{
					this->dump_memories_data = true;
					delete fir;
				}
				else if (fir->cmd == vp::FICommand::GET_REG_DEV_DATA)
				{
					this->dump_regfiles_data = true;
					delete fir;
				}
				else if (fir->cmd == vp::FICommand::GET_INTERCO_DATA)
				{
					this->dump_interco_data = true;
					delete fir;
				}
				else if (fir->cmd == vp::FICommand::GET_PREFETCHERS_DATA)
				{
					this->dump_prefetchers_data = true;
					delete fir;
				}
				else if (fir->cmd == vp::FICommand::GET_CACHES_DATA)
				{
					this->dump_caches_data = true;
					delete fir;
				}
				else
				{
					fprintf(stderr, "Unknown command type %d!\n", fir->cmd);
				}
			}
		}
	}		
}

vp::FIRequest *FIC::assemble_request(int cmd, int kind, int target_type, 
	int target, uint64_t addr, uint8_t bit, uint64_t duration, uint8_t is_high, 
	uint64_t delay, uint64_t size, uint64_t id)
{
	vp::FIRequest *fir = new vp::FIRequest();

	fir->cmd 			= (vp::FICommand) cmd;
	fir->target_type 	= (vp::FITargetType) target_type;
	fir->fault_type 	= (vp::FIFaultType) kind;
	fir->target			= target;
	fir->addr 			= addr;
	fir->bit 			= bit;
	fir->delay 			= delay;
	fir->id				= id;
	fir->size 			= size;
	fir->is_high 		= (is_high == 1);
	fir->duration 		= duration;
	fir->to_remove 		= false;

	return fir;
}

/*
 *	The fault request structure should be freed here.	
 */
void FIC::inject_fault(vp::FIRequest *fir)
{
	//printf("[FIC::inject_fault@%s] enter target=%d, addr=0x%x, bit=%x, cycles=%lu\n", this->get_path().c_str(), fir->target, fir->addr, fir->bit, this->clock.get_cycles());

	if (fir->fault_type == vp::FIFaultType::BITFLIP) 
	{
		if (fir->target_type == vp::FITargetType::MEMORY)
		{
			uint8_t mask = (uint8_t) (((1 << fir->size) - 1) << fir->bit);
			
			if (fir->target == VP_GLOBAL_AS)
			{
				vp::IoReq req;
				req.init();
				req.set_addr(fir->addr);
				req.set_size(1);
				req.debug = true;
				req.fault_upset_request = true;
				req.mask = mask;

				int err = this->global_as.req(&req);
				if (err != vp::IO_REQ_OK)
				{
					fprintf(stderr, "Could not issue fault into global address space!\n");
				}
			}
			else
			{
				uint64_t pos 	= fir->addr;
				uint64_t mem_id = (uint64_t) fir->target;

				if (mem_id >= this->memory_sizes.size())
				{
					fprintf(stderr, "Fault injection target %lu out-of-range!\n", mem_id);
				}
				else if (pos >= this->memory_sizes[mem_id])
				{
					fprintf(stderr, "Address of the faulted byte (0x%x) is out of bound!\n", pos);
				}
				else
				{
					uint8_t data = this->memory_ptrs[mem_id][pos];
					this->memory_ptrs[mem_id][pos] = data ^ mask;
					//printf("Applied multi-bit upset 0x%x -> 0x%x at 0x%x in %s\n", data, this->memory_ptrs[mem_id][pos], pos, this->memory_paths[mem_id].c_str());
				}
			}
		}
		else if (fir->target_type == vp::FITargetType::REGISTER)
		{
			uint64_t reg = fir->addr;
			uint64_t regfile_id = fir->target;

			if (regfile_id >= this->regfile_sizes.size())
			{
				fprintf(stderr, "Fault injection target %lu out-of-range!\n", regfile_id);
			} 
			else if (reg >= this->regfile_nbs[reg])
			{
				fprintf(stderr, "Register ID %lu outside of the regfile scope!\n", reg);
			}
			else
			{
				if (this->regfile_sizes[regfile_id] == 4)
				{
					uint32_t mask = (uint32_t) (((1 << fir->size) - 1) << fir->bit);
					uint32_t *regfile = (uint32_t *) this->regfile_ptrs[regfile_id];
					uint32_t data = regfile[reg];
					regfile[reg] = data ^ mask;
					//printf("Applied bitflip 0x%x -> 0x%x to register %lu in %s @ 0x%p\n", data, ((uint32_t *) this->regfile_ptrs[regfile_id])[reg], regfile_id, this->regfile_paths[regfile_id].c_str(), regfile);
				} 
				else
				{
					uint64_t mask = (uint64_t) (((1 << fir->size) - 1) << fir->bit);
					uint64_t *regfile = (uint64_t *) this->regfile_ptrs[regfile_id];
					uint64_t data = regfile[reg];
					regfile[reg] = data ^ mask;
					//printf("Applied bitflip 0x%x -> 0x%x to register %lu in %s @ 0x%p\n", data, ((uint64_t *) this->regfile_ptrs[regfile_id])[reg], regfile_id, this->regfile_paths[regfile_id].c_str(), regfile);
				}
			}
		}
		else 	if (fir->target_type == vp::FITargetType::PREFETCHER)
		{
			uint64_t byte = fir->addr;
			uint64_t pref_id = fir->target;

			if (pref_id >= this->prefetcher_sizes.size())
			{
				fprintf(stderr, "Fault injection prefetcher target %lu out-of-range!\n", pref_id);
			}	
			else if (byte >= this->prefetcher_sizes[pref_id])
			{
				fprintf(stderr, "Byte position %lu is outside of the prefetcher buffer!\n", byte);
			}
			else
			{
				uint64_t mask = (uint64_t) (((1 << fir->size) - 1) << fir->bit);
				uint8_t *prefetcher_buffer = this->prefetcher_ptrs[pref_id];
				prefetcher_buffer[byte] ^= mask;
				  
			}
		}
		else if (fir->target_type == vp::FITargetType::CACHE_DATA
				  || fir->target_type == vp::FITargetType::CACHE_TAG
			  	  || fir->target_type == vp::FITargetType::CACHE_DB)
		{
			uint64_t line = fir->addr;
			uint64_t cache_id = fir->target;
			
			if (cache_id >= this->caches_line_sizes.size())
			{
				fprintf(stderr, "Fault injection cache target %u out-of-range!\n", cache_id);
			}

			uint64_t line_size = this->caches_line_sizes[cache_id];
			uint64_t struct_size = this->caches_struct_sizes[cache_id];
			uint8_t *cache_lines = this->caches_lines[cache_id];

			/*
			 *	In case of data field, encode both the line number
			 *	and the byte number into the fir->addr field by
			 *	specifying the 'overall' byte index, i.e.:
			 *
			 *	fir->addr = line_nr * line_sz + byte_inside_line
			 *
			 *	Otherwise, fir->addr denotes the line number.
			 */
			if (fir->target_type == vp::FITargetType::CACHE_DATA)
			{
				line /= line_size;
			}

			uint8_t *cache_line = &(cache_lines[struct_size * line]);

			if (fir->target_type == vp::FITargetType::CACHE_DATA)
			{
				uint8_t mask = (uint8_t) (((1 << fir->size) - 1) << fir->bit);

				uint64_t data_off = this->caches_data_off[cache_id];
				uint64_t byte_off = fir->addr % line_size;

				uint8_t **data_ptr = (uint8_t **) &(cache_line[data_off]);
				uint8_t *data = *data_ptr;
				data[byte_off] = (data[byte_off]) ^ mask;
			}
			else if (fir->target_type == vp::FITargetType::CACHE_TAG)
			{
				uint32_t mask = (uint32_t) (((1 << fir->size) - 1) << fir->bit);

				uint64_t tag_off = this->caches_tag_off[cache_id];
				uint32_t *tag = (uint32_t *) &(cache_line[tag_off]);
				*tag = (*tag) ^ mask;
			}
			else if (fir->target_type == vp::FITargetType::CACHE_DB)
			{
				uint64_t dirty_off = this->caches_dirty_off[cache_id];
				bool *dirty = (bool *) &(cache_line[dirty_off]);
				*dirty = !(*dirty);
			}
		}
	}
	else if (fir->fault_type == vp::FIFaultType::STUCK_AT)
	{
		if (fir->target_type == vp::FITargetType::MEMORY)
		{
			uint64_t pos 	= fir->addr;
			uint64_t mem_id = (uint64_t) fir->target;

			if (mem_id >= this->memory_sizes.size())
			{
				fprintf(stderr, "Fault injection target %lu out-of-range!\n", mem_id);
			}
			else if (pos >= this->memory_sizes[mem_id])
			{
				fprintf(stderr, "Address of the faulted byte (0x%x) is out of bound!\n", pos);
			}

			std::map<uint64_t, std::vector<vp::FIRequest *>>& stuck_at_map = 
				this->memory_stuck_at_maps[mem_id].get();	

			if (fir->to_remove)
			{
				auto it = stuck_at_map.find(fir->addr);
				if (it != stuck_at_map.end())
				{
					auto& vec = it->second;
					for (auto vit = vec.begin(); vit != vec.end(); )
					{
						vp::FIRequest *tf = *vit;
						if (tf->bit == fir->bit
							&& tf->duration == fir->duration
							&& tf->is_high == fir->is_high)
						{
							delete tf;
							vit = vec.erase(vit);
						}
						else
						{
							++vit;
						}
					}

					// Remove the bucket if the list is empty
					if (vec.empty())
					{
						stuck_at_map.erase(it);
					}
				}
			}
			else
			{
				// We delete fir in caller, but it has to live, so copy
				vp::FIRequest *tf = fir_deep_copy(fir);
				
				if (fir->duration != VP_FI_PERMANENT)
				{
					// Enqueue corresponding request to lift the fault
					vp::FIRequest *removal_req 	= fir_deep_copy(fir);
					removal_req->to_remove = true;
					removal_req->delay = fir->duration;
					this->enqueue_fault_dynamic(removal_req);
				}

				// Apply fault by manipulating the stuck_at map at target
				stuck_at_map[fir->addr].push_back(tf);
			}
		}
		else
		{
			fprintf(stderr, "Stuck-at's implemented only for memories!\n");
		}
	}
}

void FIC::enqueue_fault_dynamic(vp::FIRequest *fir)
{
	uint64_t delay = fir->delay;
	uint64_t target_cycle = delay + this->clock.get_cycles();

	fir->target_cycle = target_cycle;

	if (!this->event->is_enqueued())
	{
		this->event->enqueue(delay);
	} 
	else if (this->event->get_cycle() > target_cycle)
	{
		this->event->cancel();
		this->event->enqueue(delay);
	}

	this->fault_queue.push(fir);
}

void FIC::reset(bool active)
{
	if (active && !this->static_setup_done) 
	{	
		if (!this->fault_queue.empty())
		{
			vp::FIRequest *front = this->fault_queue.top();

			if (!this->event->is_enqueued())
			{
				this->event->enqueue(front->delay);
			}
			else if (this->event->get_cycle() > front->target_cycle)
			{
				this->event->cancel();
				this->event->enqueue(front->delay);
			}
		}

		this->static_setup_done = true;
	}
}

/*
 *	- We are woken up because we must issue some faults right now.
 *	- So go over both fault queues and drain them accordingly.
 *	- Finally, set the timer to the closest event in either queue.
 */
void FIC::handle_event(vp::Block *__this, vp::ClockEvent *event)
{
	FIC *_this = (FIC *)__this;
	uint64_t curr_cycle = _this->clock.get_cycles();

	while (!_this->fault_queue.empty())
	{
		vp::FIRequest *front = _this->fault_queue.top();

		if (front->target_cycle == curr_cycle) 
		{
			_this->fault_queue.pop();
			_this->inject_fault(front);
			delete front;
		}
		else
		{
			break;
		}
	}

	if (!_this->fault_queue.empty())
	{
		uint64_t next_ts = _this->fault_queue.top()->target_cycle - curr_cycle;
		_this->event->enqueue(next_ts);
	}
}
  
void FIC::stop()
{
	/* For file handling */
	std::string sanitized = this->get_path();
	std::replace(sanitized.begin(), sanitized.end(), '/', '_');

	if (!this->hash_requests.empty())
	{
		std::ofstream file("hashes" + sanitized);

		while (!this->hash_requests.empty())
		{
			vp::FIRequest *hr = this->hash_requests.front();
			this->hash_requests.pop();

			int target 		= hr->target;
			uint64_t addr 	= hr->addr;
			uint64_t size 	= hr->size;


			if (target == -1)
			{
				uint8_t *buffer = new uint8_t[size];

				vp::IoReq req{};

				req.init();
				req.set_addr(addr);
				req.set_size(size);
				req.set_data(buffer);
				req.set_is_write(false);
				req.hash_request = true;
				req.debug = true;

				uint64_t hash = 0;
				int err = this->global_as.req(&req);
				if (err == vp::IO_REQ_OK) 
				{
					hash = hash_data(buffer, size);
				}

				file << std::to_string(hr->id) << " " << std::to_string(hash) << "\n"; 

				delete[] buffer;
				delete hr;
			}
			else
			{
				uint64_t mem_id = (uint64_t) target;

				if (mem_id >= this->memory_sizes.size())
				{
					fprintf(stderr, "Target device %d for hashing is out-of-range!\n", mem_id);
				}
				else if (addr + size >= this->memory_sizes[mem_id])
				{
					fprintf(stderr, "Address of memory region to hash is out-of-bound!\n");
				}

				uint8_t *start = this->memory_ptrs[mem_id];
				uint8_t *to_hash = &(start[addr]);

				uint64_t hash = hash_data(to_hash, size);

				file << std::to_string(hr->id) << " " << std::to_string(hash) << "\n"; 

				delete hr;
			}
		}
	}

	if (this->dump_final_cycle_count)
	{
		std::ofstream file("cycle_count" + sanitized);
		file << std::to_string(this->clock.get_cycles()) << "\n";
	}

	if (this->dump_memories_data)
	{
		uint64_t size;
		std::string path;
		
		std::ofstream file("memories_data" + sanitized);

		for (uint64_t i = 0; i < this->curr_memory_id; i++)
		{
			size = this->memory_sizes[i];	
			path = this->memory_paths[i];
			file << std::to_string(i) << " " << path.c_str() << " " << std::to_string(size) << "\n";
		}
	}

	if (this->dump_regfiles_data)
	{
		uint64_t nb_regs;
		uint64_t size;
		int type;
		std::string path;

		std::ofstream file("regfiles_data" + sanitized);

		for (uint64_t i = 0; i < this->curr_regfile_id; i++)
		{
			nb_regs = this->regfile_nbs[i];
			size 	= this->regfile_sizes[i];
			type	= this->regfile_types[i];
			path 	= this->regfile_paths[i];
			file << std::to_string(i) << " " << path.c_str() << " " 
			  << std::to_string(type) << " " << std::to_string(size) 
			  << " " << std::to_string(nb_regs) << "\n";
		}
	}

	if (this->dump_interco_data)
	{
		std::vector<vp::MappingTreeEntry *> nodes;
		std::string path;

		std::ofstream file("interco_data" + sanitized);		

		for (uint64_t i = 0; i < this->curr_interco_id; i++)
		{
			path = this->interco_paths[i];
			nodes = this->interco_links[i];

			file << path.c_str() << "\n";	
			
			for (auto entry : nodes)
			{
				std::string name 	= entry->name;
				int id 				= entry->id;
				uint64_t base 		= entry->base;
				uint64_t size 		= entry->size;
				file << name.c_str() << " " << std::to_string(id) << " "
				  << std::to_string(base) << " " << std::to_string(size) << "\n";
			}
		}
	}

	if (this->dump_prefetchers_data)
	{
		uint64_t size;
		std::string path;

		std::ofstream file("prefetchers_data" + sanitized);

		for (uint64_t i = 0; i < this->curr_prefetcher_id; i++)
		{
			size 	= this->prefetcher_sizes[i];
			path 	= this->prefetcher_paths[i];
			file << std::to_string(i) << " " << path.c_str() << " " 
			  << std::to_string(size) << "\n";
		}
	}

	if (this->dump_caches_data)
	{
		uint64_t nb_lines, line_size, tag_bits;
		std::string path;

		std::ofstream file("caches_data" + sanitized);

		for (uint64_t i = 0; i < this->curr_cache_id; i++)
		{
			nb_lines = this->caches_nb_lines[i];
			line_size = this->caches_line_sizes[i];
			tag_bits = this->caches_tag_bitwidths[i];
			path = this->caches_paths[i];
			file << std::to_string(i) << " " << path.c_str() << " "
			  << std::to_string(nb_lines) << " " << std::to_string(line_size)
			  << " " << std::to_string(tag_bits) << "\n";
		}
	}
}

// FNV hash
uint64_t FIC::hash_data(uint8_t *start, uint64_t len)
{
	uint64_t hash = 14695981039346656037ULL; // FNV offset basis

	for (uint64_t i = 0; i < len; i++) {
		hash ^= start[i];
		hash *= 1099511628211ULL; // FNV prime
	}

	return hash;
}

vp::FIRequest *FIC::fir_deep_copy(vp::FIRequest *other)
{
	vp::FIRequest *copy = new vp::FIRequest();

    copy->cmd 			= other->cmd;
    copy->fault_type 	= other->fault_type;
    copy->target 		= other->target;
    copy->addr 			= other->addr;
    copy->bit 			= other->bit;
    copy->delay 		= other->delay;
    copy->duration 		= other->duration;
    copy->target_cycle 	= other->target_cycle;
    copy->id 			= other->id;
    copy->size 			= other->size;
    copy->is_high 		= other->is_high;
    copy->to_remove 	= other->to_remove;

	return copy;
}

void FIC::register_memory(vp::Component *comp, uint8_t *memory_ptr, 
	uint64_t memory_size, StuckAtMap& stuck_at_map)
{
	this->memory_ptrs.push_back(memory_ptr);
	this->memory_sizes.push_back(memory_size);
	this->memory_paths.push_back(comp->get_path());
	this->memory_stuck_at_maps.push_back(stuck_at_map);
	this->curr_memory_id += 1;
}

void FIC::register_regfile(vp::Component *comp, int type, void *regfile, 
	uint64_t nb_regs, uint64_t size)
{
	this->regfile_ptrs.push_back(regfile);
	this->regfile_nbs.push_back(nb_regs);
	this->regfile_sizes.push_back(size);
	this->regfile_types.push_back(type);
	this->regfile_paths.push_back(comp->get_path());
	this->curr_regfile_id += 1;
}

void FIC::register_interco(vp::Component *comp, vp::MappingTree *mapping_tree)
{
	this->interco_links.push_back(mapping_tree->get_entries_copy());
	this->interco_paths.push_back(comp->get_path());
	this->curr_interco_id += 1;
}

void FIC::register_prefetcher(vp::Component *comp, uint8_t *prefetcher_buffer, 
	uint64_t prefetcher_size)
{
	this->prefetcher_ptrs.push_back(prefetcher_buffer);
	this->prefetcher_sizes.push_back(prefetcher_size);
	this->prefetcher_paths.push_back(comp->get_path());
	this->curr_prefetcher_id += 1;
}

void FIC::register_cache(vp::Component *comp, uint8_t *lines, uint64_t nb_lines,
	uint64_t line_size_bits, uint64_t nb_sets_bits, uint64_t struct_size, 
	uint64_t tag_off, uint64_t dirty_off, uint64_t data_off)
{
	uint64_t line_size = 1 << line_size_bits;
	uint64_t tag_bits = ADDR_BITS - line_size_bits - nb_sets_bits;

	this->caches_lines.push_back(lines);
	this->caches_nb_lines.push_back(nb_lines);

	this->caches_line_sizes.push_back(line_size);
	this->caches_tag_bitwidths.push_back(tag_bits);

	this->caches_struct_sizes.push_back(struct_size);
	this->caches_tag_off.push_back(tag_off);
	this->caches_dirty_off.push_back(dirty_off);
	this->caches_data_off.push_back(data_off);

	this->caches_paths.push_back(comp->get_path());

	this->curr_cache_id += 1;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
	return new FIC(config);
}
