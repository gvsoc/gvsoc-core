// Copyright (c) 2010-2017, The Regents of the University of California
// (Regents).  All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the Regents nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
// SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
// OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
// HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
// MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/time.h>
#include <stdint.h>
#include <vp/vp.hpp>
#include <queue>
#include <vp/itf/io.hpp>
#include <thread>
#include <vp/controller.hpp>

#define UART_QUEUE_SIZE 64

#define UART_RX 0 /* In:  Receive buffer */
#define UART_TX 0 /* Out: Transmit buffer */

#define UART_IER 1         /* Out: Interrupt Enable Register */
#define UART_IER_MSI 0x08  /* Enable Modem status interrupt */
#define UART_IER_RLSI 0x04 /* Enable receiver line status interrupt */
#define UART_IER_THRI 0x02 /* Enable Transmitter holding register int. */
#define UART_IER_RDI 0x01  /* Enable receiver data interrupt */

#define UART_IIR 2           /* In:  Interrupt ID Register */
#define UART_IIR_NO_INT 0x01 /* No interrupts pending */
#define UART_IIR_ID 0x0e     /* Mask for the interrupt ID */
#define UART_IIR_MSI 0x00    /* Modem status interrupt */
#define UART_IIR_THRI 0x02   /* Transmitter holding register empty */
#define UART_IIR_RDI 0x04    /* Receiver data interrupt */
#define UART_IIR_RLSI 0x06   /* Receiver line status interrupt */

#define UART_IIR_TYPE_BITS 0xc0

#define UART_FCR 2                /* Out: FIFO Control Register */
#define UART_FCR_ENABLE_FIFO 0x01 /* Enable the FIFO */
#define UART_FCR_CLEAR_RCVR 0x02  /* Clear the RCVR FIFO */
#define UART_FCR_CLEAR_XMIT 0x04  /* Clear the XMIT FIFO */
#define UART_FCR_DMA_SELECT 0x08  /* For DMA applications */

#define UART_LCR 3           /* Out: Line Control Register */
#define UART_LCR_DLAB 0x80   /* Divisor latch access bit */
#define UART_LCR_SBC 0x40    /* Set break control */
#define UART_LCR_SPAR 0x20   /* Stick parity (?) */
#define UART_LCR_EPAR 0x10   /* Even parity select */
#define UART_LCR_PARITY 0x08 /* Parity Enable */
#define UART_LCR_STOP 0x04   /* Stop bits: 0=1 bit, 1=2 bits */

#define UART_MCR 4         /* Out: Modem Control Register */
#define UART_MCR_LOOP 0x10 /* Enable loopback test mode */
#define UART_MCR_OUT2 0x08 /* Out2 complement */
#define UART_MCR_OUT1 0x04 /* Out1 complement */
#define UART_MCR_RTS 0x02  /* RTS complement */
#define UART_MCR_DTR 0x01  /* DTR complement */

#define UART_LSR 5                   /* In:  Line Status Register */
#define UART_LSR_FIFOE 0x80          /* Fifo error */
#define UART_LSR_TEMT 0x40           /* Transmitter empty */
#define UART_LSR_THRE 0x20           /* Transmit-hold-register empty */
#define UART_LSR_BI 0x10             /* Break interrupt indicator */
#define UART_LSR_FE 0x08             /* Frame error indicator */
#define UART_LSR_PE 0x04             /* Parity error indicator */
#define UART_LSR_OE 0x02             /* Overrun error indicator */
#define UART_LSR_DR 0x01             /* Receiver data ready */
#define UART_LSR_BRK_ERROR_BITS 0x1E /* BI, FE, PE, OE bits */

#define UART_MSR 6              /* In:  Modem Status Register */
#define UART_MSR_DCD 0x80       /* Data Carrier Detect */
#define UART_MSR_RI 0x40        /* Ring Indicator */
#define UART_MSR_DSR 0x20       /* Data Set Ready */
#define UART_MSR_CTS 0x10       /* Clear to Send */
#define UART_MSR_DDCD 0x08      /* Delta DCD */
#define UART_MSR_TERI 0x04      /* Trailing edge ring indicator */
#define UART_MSR_DDSR 0x02      /* Delta DSR */
#define UART_MSR_DCTS 0x01      /* Delta CTS */
#define UART_MSR_ANY_DELTA 0x0F /* Any of the delta bits! */

#define UART_SCR 7 /* I/O: Scratch Register */



class Ns16550 : public vp::Component
{
public:
    Ns16550(vp::ComponentConf &conf);

    void reset(bool active);
    void stop();

private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    bool load(unsigned int addr, size_t len, uint8_t *bytes);
    bool store(unsigned int addr, size_t len, const uint8_t *bytes);
    int read(bool blocking=false);
    void write(char ch);
    void stdin_task();

    vp::Trace trace;
    uint32_t reg_shift;
    uint32_t reg_io_width;
    std::queue<uint8_t> rx_queue;
    uint8_t dll;
    uint8_t dlm;
    uint8_t iir;
    uint8_t ier;
    uint8_t fcr;
    uint8_t lcr;
    uint8_t mcr;
    uint8_t lsr;
    uint8_t msr;
    uint8_t scr;
    void update_interrupt(void);
    uint8_t rx_byte(void);
    void tx_byte(uint8_t val);

    vp::IoSlave input_itf;
    vp::WireMaster<bool> irq_itf;

    struct termios old_tios;
    bool restore_tios;

    std::thread *stdin_thread;
};




void Ns16550::stop()
{
    if (restore_tios)
        tcsetattr(0, TCSANOW, &old_tios);
}



void Ns16550::reset(bool active)
{
}

vp::IoReqStatus Ns16550::req(vp::Block *__this, vp::IoReq *req)
{
    Ns16550 *_this = (Ns16550 *)__this;
    bool status;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received request (offset: 0x%x, size: 0x%d, is_write: %d)\n",
        req->get_addr(), req->get_size(), req->get_is_write());

    if (req->get_is_write())
    {
        status = _this->store(req->get_addr(), req->get_size(), req->get_data());
    }
    else
    {
        status = _this->load(req->get_addr(), req->get_size(), req->get_data());
    }
    return status ? vp::IO_REQ_OK : vp::IO_REQ_INVALID;
}



void Ns16550::update_interrupt(void)
{
    uint8_t interrupts = 0;

    /* Handle clear rx */
    if (lcr & UART_FCR_CLEAR_RCVR)
    {
        lcr &= ~UART_FCR_CLEAR_RCVR;
        while (!rx_queue.empty())
        {
            rx_queue.pop();
        }
        lsr &= ~UART_LSR_DR;
    }

    /* Handle clear tx */
    if (lcr & UART_FCR_CLEAR_XMIT)
    {
        lcr &= ~UART_FCR_CLEAR_XMIT;
        lsr |= UART_LSR_TEMT | UART_LSR_THRE;
    }

    /* Data ready and rcv interrupt enabled ? */
    if ((ier & UART_IER_RDI) && (lsr & UART_LSR_DR))
    {
        interrupts |= UART_IIR_RDI;
    }

    /* Transmitter empty and interrupt enabled ? */
    if ((ier & UART_IER_THRI) && (lsr & UART_LSR_TEMT))
    {
        interrupts |= UART_IIR_THRI;
    }

    /* Now update the interrup line, if necessary */
    if (!interrupts)
    {
        iir = UART_IIR_NO_INT;
        this->irq_itf.sync(false);
    }
    else
    {
        iir = interrupts;
        this->irq_itf.sync(true);
    }

    /*
     * If the OS disabled the tx interrupt, we know that there is nothing
     * more to transmit.
     */
    if (!(ier & UART_IER_THRI))
    {
        lsr |= UART_LSR_TEMT | UART_LSR_THRE;
    }
}

uint8_t Ns16550::rx_byte(void)
{
    if (rx_queue.empty())
    {
        lsr &= ~UART_LSR_DR;
        return 0;
    }

    /* Break issued ? */
    if (lsr & UART_LSR_BI)
    {
        lsr &= ~UART_LSR_BI;
        return 0;
    }

    uint8_t ret = rx_queue.front();
    rx_queue.pop();
    if (rx_queue.empty())
    {
        lsr &= ~UART_LSR_DR;
    }

    return ret;
}

void Ns16550::tx_byte(uint8_t val)
{
    lsr |= UART_LSR_TEMT | UART_LSR_THRE;
    this->write(val);
}


bool Ns16550::load(unsigned int addr, size_t len, uint8_t *bytes)
{
    uint8_t val;
    bool ret = true, update = false;

    if (reg_io_width != len)
    {
        return false;
    }
    addr >>= reg_shift;
    addr &= 7;

    switch (addr)
    {
    case UART_RX:
        if (lcr & UART_LCR_DLAB)
        {
            val = dll;
        }
        else
        {
            val = rx_byte();
        }
        update = true;
        break;
    case UART_IER:
        if (lcr & UART_LCR_DLAB)
        {
            val = dlm;
        }
        else
        {
            val = ier;
        }
        break;
    case UART_IIR:
        val = iir | UART_IIR_TYPE_BITS;
        break;
    case UART_LCR:
        val = lcr;
        break;
    case UART_MCR:
        val = mcr;
        break;
    case UART_LSR:
        val = lsr;
        break;
    case UART_MSR:
        val = msr;
        break;
    case UART_SCR:
        val = scr;
        break;
    default:
        ret = false;
        break;
    };

    if (ret)
    {
        bytes[0] = val;
    }
    if (update)
    {
        update_interrupt();
    }

    return ret;
}

bool Ns16550::store(unsigned int addr, size_t len, const uint8_t *bytes)
{
    uint8_t val;
    bool ret = true, update = false;
    if (reg_io_width != len)
    {
        return false;
    }
    addr >>= reg_shift;
    addr &= 7;
    val = bytes[0];

    switch (addr)
    {
    case UART_TX:
        update = true;

        if (lcr & UART_LCR_DLAB)
        {
            dll = val;
            break;
        }

        /* Loopback mode */
        if (mcr & UART_MCR_LOOP)
        {
            if (rx_queue.size() < UART_QUEUE_SIZE)
            {
                rx_queue.push(val);
                lsr |= UART_LSR_DR;
            }
            break;
        }

        tx_byte(val);
        break;
    case UART_IER:
        if (!(lcr & UART_LCR_DLAB))
        {
            ier = val & 0x0f;
        }
        else
        {
            dlm = val;
        }
        update = true;
        break;
    case UART_FCR:
        fcr = val;
        update = true;
        break;
    case UART_LCR:
        lcr = val;
        update = true;
        break;
    case UART_MCR:
        mcr = val;
        update = true;
        break;
    case UART_LSR:
        /* Factory test */
        break;
    case UART_MSR:
        /* Not used */
        break;
    case UART_SCR:
        scr = val;
        break;
    default:
        ret = false;
        break;
    };

    if (update)
    {
        update_interrupt();
    }

    return ret;
}


void Ns16550::stdin_task(void)
{
    while (1)
    {
        int rc = this->read(true);
        if (rc < 0) return;

        gv::Controller::get().engine_lock();

        while (!(this->fcr & UART_FCR_ENABLE_FIFO) ||
            (this->mcr & UART_MCR_LOOP) ||
            (UART_QUEUE_SIZE <= this->rx_queue.size()))
        {
            gv::Controller::get().engine_unlock();
            usleep(1000);
            gv::Controller::get().engine_lock();
        }

        this->rx_queue.push((uint8_t)rc);
        this->lsr |= UART_LSR_DR;

        this->update_interrupt();

        gv::Controller::get().engine_unlock();
    }
}


int Ns16550::read(bool blocking)
{
  struct pollfd pfd;
  pfd.fd = 0;
  pfd.events = POLLIN;
      int ret = poll(&pfd, 1, blocking ? -1 : 0);
  if (ret <= 0 || !(pfd.revents & POLLIN))
    return -1;

  unsigned char ch;
  ret = ::read(0, &ch, 1);
  return ret <= 0 ? -1 : ch;
}


void Ns16550::write(char ch)
{
  if (::write(1, &ch, 1) != 1)
    abort();
}

Ns16550::Ns16550(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    ier = 0;
    iir = UART_IIR_NO_INT;
    fcr = 0;
    lcr = 0;
    lsr = UART_LSR_TEMT | UART_LSR_THRE;
    msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS;
    dll = 0x0C;
    mcr = UART_MCR_OUT2;
    scr = 0;

    reg_io_width = 1;

    this->reg_shift = this->get_js_config()->get_child_int("offset_shift");

    this->input_itf.set_req_meth(&Ns16550::req);
    new_slave_port("input", &this->input_itf);

    this->new_master_port("irq", &this->irq_itf);

    this->restore_tios = false;
    if (tcgetattr(0, &old_tios) == 0)
    {
        struct termios new_tios = old_tios;
        new_tios.c_lflag &= ~(ICANON | ECHO);
        if (tcsetattr(0, TCSANOW, &new_tios) == 0)
            restore_tios = true;
    }

    this->stdin_thread = new std::thread(&Ns16550::stdin_task, this);

}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Ns16550(config);
}
