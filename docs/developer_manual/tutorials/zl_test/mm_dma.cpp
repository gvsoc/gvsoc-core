#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>

class mm_dma : public vp::Component
{

public:
    mm_dma(vp::ComponentConf &config);
    static void handle_rsp(vp::Block *__this, vp::IoReq *req);
    static void handle_grt(vp::Block *__this, vp::IoReq *req);

private:
    static vp::IoReqStatus handle_req(vp::Block *__this, vp::IoReq *req);
    static void read_transfer_handler(vp::Block *__this, vp::ClockEvent *event);
    static void write_transfer_handler(vp::Block *__this, vp::ClockEvent *event);

    vp::IoSlave input_itf;
    vp::IoMaster output_itf;


    vp::IoReq dma_req;    
    uint32_t start;
    uint32_t src_addr;
    uint32_t dst_addr;
    uint32_t transfer_size;
    uint32_t curr_size;
    uint8_t *data;

    vp::ClockEvent *read_event;
    vp::ClockEvent *write_event;
    

    vp::Trace trace;
};


mm_dma::mm_dma(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->input_itf.set_req_meth(&mm_dma::handle_req);
    this->new_slave_port("input", &this->input_itf);
    
    this->output_itf.set_resp_meth(&mm_dma::handle_rsp);
    this->output_itf.set_grant_meth(&mm_dma::handle_grt);
    this->new_master_port("output", &this->output_itf);

    this->start=0x0;
    this->src_addr=0x0;
    this->dst_addr=0x0;
    this->transfer_size=0x0;
    this->curr_size=0x0;

    this->data=(uint8_t*)calloc(4,sizeof(uint8_t));

    this->read_event = this->event_new(mm_dma::read_transfer_handler);
    this->write_event = this->event_new(mm_dma::write_transfer_handler);

    this->traces.new_trace("mm_dma_trace", &this->trace);

}

void mm_dma::handle_rsp(vp::Block *__this, vp::IoReq *req) {
    mm_dma *_this = (mm_dma *)__this;
    _this->trace.msg(vp::TraceLevel::DEBUG, "DMA RECEIVED RESPONSE\n");
    if (_this->dma_req.get_is_write())
        _this->event_enqueue(_this->write_event, _this->dma_req.get_full_latency());
    else
        _this->event_enqueue(_this->read_event, _this->dma_req.get_full_latency());
}

void mm_dma::handle_grt(vp::Block *__this, vp::IoReq *req) {
}


void mm_dma::read_transfer_handler(vp::Block *__this, vp::ClockEvent *event) {
    mm_dma *_this = (mm_dma *)__this;

    vp::IoReqStatus dma_err;
    int64_t latency = 1;

    if (_this->curr_size<_this->transfer_size) {
        _this->trace.msg(vp::TraceLevel::DEBUG, "DMA READ REQ\n");

        _this->dma_req.init();
        _this->dma_req.set_addr(_this->src_addr+_this->curr_size);
        _this->dma_req.set_data(_this->data);
        _this->dma_req.set_size(4);
        _this->dma_req.set_is_write(false);
        dma_err = _this->output_itf.req(&_this->dma_req);
        if (dma_err == vp::IO_REQ_OK) {
            latency += _this->dma_req.get_full_latency();
            _this->event_enqueue(_this->write_event, latency); //after a read we enqueue a write
        }
        else {
            if (dma_err == vp::IO_REQ_INVALID) {
                _this->trace.force_warning("Received error during in DMA transacition\n");
            }
        }
    }
    else {
        _this->trace.msg(vp::TraceLevel::DEBUG, "DMA COMPLETED\n");
        _this->src_addr=0x0;
        _this->dst_addr=0x0;
        _this->transfer_size=0x0;
        _this->curr_size=0x0;
        _this->start=0x0;
    }
}

void mm_dma::write_transfer_handler(vp::Block *__this, vp::ClockEvent *event) {
    mm_dma *_this = (mm_dma *)__this;

    vp::IoReqStatus dma_err;
    int64_t latency = 1;

    _this->trace.msg(vp::TraceLevel::DEBUG, "DMA WRITE REQ\n");

    _this->dma_req.init();
    _this->dma_req.set_addr(_this->dst_addr+_this->curr_size);
    _this->dma_req.set_data(_this->data);
    _this->dma_req.set_size(4);
    _this->dma_req.set_is_write(true);
    dma_err = _this->output_itf.req(&_this->dma_req);
    if (dma_err == vp::IO_REQ_OK) {
        _this->curr_size++; //when write is completed we increment the addr
        latency += _this->dma_req.get_full_latency();
        _this->event_enqueue(_this->read_event, latency); //a write enqueues a read
    }
    else {
        if (dma_err == vp::IO_REQ_INVALID) {
            _this->trace.force_warning("Received error during in DMA transacition\n");
        }
    }
}

vp::IoReqStatus mm_dma::handle_req(vp::Block *__this, vp::IoReq *req)
{
    mm_dma *_this = (mm_dma *)__this;

    _this->trace.msg(vp::TraceLevel::DEBUG, "Received request at offset 0x%lx, size 0x%lx, is_write %d\n",
        req->get_addr(), req->get_size(), req->get_is_write());

    if (req->get_size() == 4)
    {
        if (req->get_is_write()) { //WRITE
            if (req->get_addr()==0x0) { // reg src_addr
                _this->src_addr=*(uint32_t *)req->get_data();
                _this->trace.msg(vp::TraceLevel::DEBUG, "Write DMA SRC ADDR at 0x%08x\n",_this->src_addr);
                req->inc_latency(2);
                return vp::IO_REQ_OK;
            }
            else if (req->get_addr()==0x4) { // reg dst_addr
                _this->dst_addr=*(uint32_t *)req->get_data();
                _this->trace.msg(vp::TraceLevel::DEBUG, "Write DMA DST ADDR at 0x%08x\n",_this->dst_addr);
                req->inc_latency(2);
                return vp::IO_REQ_OK;
            }
            else if (req->get_addr()==0x8) { // reg size
                _this->transfer_size=*(uint32_t *)req->get_data();
                _this->trace.msg(vp::TraceLevel::DEBUG, "Write DMA SIZE 0x%08x\n",_this->transfer_size);
                req->inc_latency(2);
                return vp::IO_REQ_OK;
            }
            else if (req->get_addr()==0xC) { //Start reg
                _this->start=*(uint32_t *)req->get_data();
                if (_this->start==0x1) {
                    _this->trace.msg(vp::TraceLevel::DEBUG, "DMA START\n");
                    _this->event_enqueue(_this->read_event, 1); //read event that triggers a write events
                }
                return vp::IO_REQ_OK;
            }
            else {
                return vp::IO_REQ_INVALID;
            }
        }
        else { //READ
            if (req->get_addr()==0x0) { // reg src_addr
                _this->trace.msg(vp::TraceLevel::DEBUG, "Read DMA SRC ADDR at 0x%08x\n",_this->src_addr);
                *(uint32_t *)req->get_data()=_this->src_addr;
                req->inc_latency(1);
                return vp::IO_REQ_OK;
            }
            else if (req->get_addr()==0x4) { // reg dst_addr
                _this->trace.msg(vp::TraceLevel::DEBUG, "Read DMA DST ADDR at 0x%08x\n",_this->dst_addr);
                *(uint32_t *)req->get_data()=_this->dst_addr;
                req->inc_latency(1);
                return vp::IO_REQ_OK;
            }
            else if (req->get_addr()==0x8) { // reg size
                _this->trace.msg(vp::TraceLevel::DEBUG, "Read DMA SIZE ADDR at 0x%08x\n",_this->transfer_size);
                *(uint32_t *)req->get_data()=_this->transfer_size;
                req->inc_latency(1);
                return vp::IO_REQ_OK;
            }
            else if (req->get_addr()==0xC) { //Start reg
                *(uint32_t *)req->get_data()=_this->start;
                req->inc_latency(1);
                return vp::IO_REQ_OK;
            }
            else {
                return vp::IO_REQ_INVALID;
            }
        }
    }
    return vp::IO_REQ_INVALID;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new mm_dma(config);
}
