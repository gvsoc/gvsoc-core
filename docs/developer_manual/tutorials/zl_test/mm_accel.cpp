#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>

#define BRAM_SIZE 4096 //this is defined in 64b lines

class mm_accel : public vp::Component
{

public:
    mm_accel(vp::ComponentConf &config);

private:
    static vp::IoReqStatus handle_req(vp::Block *__this, vp::IoReq *req);
    //static void handle_event(vp::Block *__this, vp::ClockEvent *event);

    vp::IoSlave input_itf;


    uint32_t *bram;

    //vp::ClockEvent event;
    //vp::IoReq *pending_req;

    vp::Trace trace;
};


mm_accel::mm_accel(vp::ComponentConf &config)
    : vp::Component(config) //, event(this, mm_accel::handle_event)
{
    this->input_itf.set_req_meth(&mm_accel::handle_req);
    this->new_slave_port("input", &this->input_itf);

    this->bram=(uint32_t*)calloc(BRAM_SIZE,sizeof(uint32_t)); //4KB empty BRAM

    //this->value = this->get_js_config()->get_child_int("value");

    this->traces.new_trace("mm_accel_trace", &this->trace);

}

// void mm_accel::handle_event(vp::Block *__this, vp::ClockEvent *event)
// {
//     mm_accel *_this = (mm_accel *)__this;

//     _this->trace.msg(vp::TraceLevel::DEBUG, "Received complention\n");

//     if (!_this->pending_req->get_is_write()) //READ
//         *(uint32_t *)_this->pending_req->get_data() = _this->bram[_this->pending_req->get_addr()&0xfff];
//     else {
//         _this->bram[_this->pending_req->get_addr()&0xfff] = *(uint32_t *)_this->pending_req->get_data();
//     }

//     _this->pending_req->get_resp_port()->resp(_this->pending_req);
// }

vp::IoReqStatus mm_accel::handle_req(vp::Block *__this, vp::IoReq *req)
{
    mm_accel *_this = (mm_accel *)__this;

    _this->trace.msg(vp::TraceLevel::DEBUG, "Received request at offset 0x%lx, size 0x%lx, is_write %d\n",
        req->get_addr(), req->get_size(), req->get_is_write());

    if (req->get_size() == 4)
    {
            // _this->pending_req = req;
            // _this->event.enqueue(4);
            // return vp::IO_REQ_PENDING;
            if ((req->get_addr()>=0x0) && (req->get_addr()<BRAM_SIZE)) { //Access to BRAM
                if (!req->get_is_write()) //READ
                    *(uint32_t *)req->get_data() = _this->bram[req->get_addr()];
                else {
                    _this->bram[req->get_addr()] = *(uint32_t *)req->get_data();
                }
                req->inc_latency(2);
                return vp::IO_REQ_OK;
            }
            

    }
    return vp::IO_REQ_INVALID;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new mm_accel(config);
}
