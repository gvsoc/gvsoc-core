#pragma once

#include <stdint.h>
#include "vp/vp.hpp"
#include "vp/itf/wire.hpp"
#include "vp/mapping_tree.hpp"

#define VP_GLOBAL_AS				-1

#define VP_FI_INJECT				0
#define VP_FI_HASH					1
#define VP_FI_GET_MEM_DEV_DATA		2
#define VP_FI_GET_CYCLES			3
#define VP_FI_GET_REG_DEV_DATA		4
#define VP_FI_GET_INTERCO_DATA		5
#define VP_FI_GET_PREFETCHERS_DATA	6

#define VP_FI_BITFLIP				0
#define VP_FI_STUCK_AT				1

#define VP_FI_PERMANENT				0

#define VP_FI_MEMORY				0
#define VP_FI_REGISTER				1
#define VP_FI_PREFETCHER			2

#define VP_FI_REG					0
#define VP_FI_FREG					1
#define VP_FI_VREG					2

namespace vp
{
	typedef enum
	{
		INJECT=0,
		HASH=1,
		GET_MEM_DEV_DATA=2,
		GET_CYCLES=3,
		GET_REG_DEV_DATA=4,
		GET_INTERCO_DATA=5,
		GET_PREFETCHERS_DATA=6,
		GET_CACHES_DATA=7,
	} FICommand;

	typedef enum
	{
		BITFLIP=0,
		STUCK_AT=1,
	} FIFaultType;

	typedef enum
	{
		MEMORY=0,
		REGISTER=1,
		PREFETCHER=2,
		CACHE_DATA=3,
		CACHE_TAG=4,
		CACHE_DB=5,
	} FITargetType;

	class FIRequest
	{
	public:
		FICommand 		cmd;	
		FIFaultType 	fault_type;
		FITargetType	target_type;
		int 			target;
		uint64_t 		addr;
		uint8_t 		bit;
		uint64_t 		delay;
		uint64_t 		duration;
		uint64_t 		target_cycle;
		uint64_t		id;
		uint64_t 		size;
		bool 			is_high;
		bool 			to_remove;
	};

	using TransientsMap = std::map<uint64_t, std::vector<vp::FIRequest *>>;

	class FIC_registrator
	{
	public:
		virtual void register_memory(vp::Component *comp, uint8_t *memory_ptr, 
			uint64_t memory_size, TransientsMap& stuck_at_map) = 0;

		virtual void register_regfile(vp::Component *comp, int type, void *regfile,
			uint64_t nb_regs, uint64_t reg_size) = 0;

		virtual void register_interco(vp::Component *comp, 
			vp::MappingTree *mapping_tree) = 0;

		virtual void register_prefetcher(vp::Component *comp,
			uint8_t *prefetch_buffer, uint64_t prefetcher_size) = 0;

		virtual void register_cache(vp::Component *comp, uint8_t *lines, uint64_t nb_lines,
			uint64_t line_size_bits, uint64_t nb_sets_bits, uint64_t struct_size,
			uint64_t tag_off, uint64_t dirty_off, uint64_t data_off) = 0;
	};
};
