#include "include/Vlsu.hpp"

class Vlsu : public vp{
public:
    int Vlsu_io_access(uint64_t addr, int size, uint8_t *data, bool is_write) override;

    void handle_pending_io_access();

    vp::io_req io_req;
    vp::clock_event *event;
    int io_retval;
    uint64_t io_pending_addr;
    int io_pending_size;
    uint8_t *io_pending_data;
    bool io_pending_is_write;
    bool waiting_io_response;
};


int Vlsu::Vlsu_io_access(uint64_t addr, int size, uint8_t *data, bool is_write)
{
    this->io_pending_addr = addr;
    this->io_pending_size = size;
    this->io_pending_data = data;
    this->io_pending_is_write = is_write;
    this->waiting_io_response = true;

    this->handle_pending_io_access();

    return this->io_retval;
}

void Vlsu::handle_pending_io_access()
{
    if (this->io_pending_size > 0){
        vp::io_req *req = &this->io_req;

        uint32_t addr = this->io_pending_addr;
        uint32_t addr_aligned = addr & ~(VLEN / 8 - 1);
        int size = addr_aligned + VLEN/8 - addr;
        if (size > this->io_pending_size){
            size = this->io_pending_size;
        }

        req->init();
        req->set_addr(addr);
        req->set_size(size);
        req->set_is_write(this->io_pending_is_write);
        req->set_data(this->io_pending_data);

        this->io_pending_data += size;
        this->io_pending_size -= size;
        this->io_pending_addr += size;

        int err = this->io_itf.req(req);
        if (err == vp::IO_REQ_OK){
            this->event->enqueue(this->io_req.get_latency() + 1);
        }
        else if (err == vp::IO_REQ_INVALID){
            this->waiting_io_response = false;
            this->io_retval = 1;
        }
        else{

        }
    }
    else{
        this->waiting_io_response = false;
        this->io_retval = 0;
    }
}