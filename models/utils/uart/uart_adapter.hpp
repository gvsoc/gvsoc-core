/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#include <queue>
#include <limits.h>
#include <vp/vp.hpp>
#include <vp/itf/uart.hpp>

/**
 * @brief UART adapter
 *
 * This adapter can be used to interact with a UART interface.\n
 * It takes care of the low-level UART protocol bit interactions and provides a high-level
 * interface for sending and receiving bytes.\n
 * The high-level interactions with the upper model are done at byte level using callbacks
 * encapsulated into timed events.\n
 */
class UartAdapter : public vp::Block
{
public:
    /**
     * @brief Construct a new UART adapter
     *
     * @param top Top component where the UART interface must be created.
     * @param parent Specify the parent block containing this block. This can be the top component
     *        if it is its direct parent.
     * @param name Specify the name of the uart adapter within the parent hierarchy.
     * @param itf_name Specify the name of the UART interface. An interface with this name will be
     *                 created in the top component. This interface must then be connected in
     *                 the python generator with the same name to another UART interface.
     */
    UartAdapter(vp::Component *top, vp::Block *parent, std::string name, std::string itf_name);

    /**
     * @brief Set the UART baudrate
     *
     * This gives the number of bits per second sent or received.
     * Baudrate is set to 115200 by default.
     *
     * @param baudrate Baudrate in number of bits per second.
     */
    void baudrate_set(int baudrate);

    /**
     * @brief Set the number of data bits
     *
     * This sets the number of data bits exchanged on the interface for each byte exchanged with the
     * upper-level model.
     * It is set to 8 by default and can only be equal or inferior to 8.
     * If it is inferior to 8, the received data bits are shifted right and filled with zero on the
     * left.
     *
     * @param data_bits Number of data bits.
     */
    void data_bits_set(int data_bits);

    /**
     * @brief Set the number of stop bits
     *
     * This sets the number of stop bits exchanged on the interface before switching to the next
     * byte.
     * It is set to 1 by default and can take any value equal or superior to zero.
     *
     * @param stop_bits Number of stop bits.
     */
    void stop_bits_set(int stop_bits);

    /**
     * @brief Set parity enable
     *
     * This enables or disables the parity bit.
     * When enabled, an additional parity bit is transmitted or received right after the data
     * bits to check the data integrity.
     * It is set to false by default.
     * The parity can be retrieved along with the received byte by calling rx_get with the parity
     * argument.
     *
     * @param parity true if parity is enabled.
     */
    void parity_set(bool parity);

    /**
     * @brief Set control flow enable
     *
     * This enables or disables the control flow.
     * The control flow is disabled by default.
     * When disabled, the cts line, is set to 0 to allow receiving
     * bytes. Once control flow is enabled, the line is set to 1 to forbid receiving bytes. The
     * upper model can then call rx_flow_enable to tell if reception is allowed or forbidden.
     *
     * @param ctrl_flow true if control flow is enabled.
     */
    void ctrl_flow_set(bool ctrl_flow);

    /**
     * @brief Set loopback enable.
     *
     * This enables or disables the loopback.
     * When enabled, received bits are handled as usual but are also immeditely mirrored on the
     * transmit line to model a loopback from RX to TX.
     *
     * @param enabled true if loopback is enabled.
     */
    void loopback_set(bool enabled);

    /**
     * @brief Set user event for receiving bytes.
     *
     * This sets the timed event which should be called everytime a byte has been received and is
     * ready to be retrieved.
     * The event is enqueued to the time scheduler with 0 delay, which means it behaves as a function
     * call.
     * The byte must be retrieved immediately. If not, it may be overwritten by the next byte.
     * The timed event is enqueued as soon as the last stop bit is received.
     *
     * @param event The user event to be enqueued everytime a byte is received.
     */
    void rx_ready_event_set(vp::TimeEvent *event);

    /**
     * @brief Get the last received byte.
     *
     * This returns the last byte received from the UART interface.
     * This must usually be called from the RX ready event callback to get the byte and let
     * the adapter capture the next one.
     *
     * @return The last received byte.
     */
    uint8_t rx_get();

    /**
     * @brief Set the flow control signal.
     *
     * This can be used to directly control the RX control flow signal on the UART interface (CTS).
     * The flow control can be enable to allow the remote part to send bytes or can be disabled to
     * forbit it.
     * The remote part may not immediately take it into account and may be still send a few bytes
     * after it has been disabled. For example, FTDI chips can usually send up to 4 bytes after
     * the flow control has been disabled.
     *
     * @param enabled True if the RX flow control must be enabled, i.e. bytes can be received.
     */
    void rx_flow_enable(bool enabled);

    /**
     * @brief Tell if the adapter is ready to send a byte.
     *
     * This basically tells if the adapter is in idle, i.e. nothing is being sent.
     * This method can be called to check if a new TX byte can be given to the adapter.
     * If a new byte is given while the adapter is not ready, some data of the previous byte may
     * be overwritten.
     * When a new TX byte is pushed, the adapter becomes ready again once the last bit of the byte
     * is sent to the interface.
     * Note that this method will only check if the adapter is ready to send. It may return true
     * even though the TX flow control (the flow control signal returned by the remote part) forbids
     * sending bytes. This can be used to model behaviors where few bytes can be sent after flow
     * control forbids sending bytes.
     *
     * @return true if a new TX byte can be given to the adapter.
     */
    bool tx_ready();

    /**
     * @brief Tell if the TX flow control allows sending bytes.
     *
     * This tells if the remote part allows the adapter to send a byte through the flow control
     * signal.
     * This always returns true when the flow control is disabled.
     * Note that the adapter does not check TX flow control signal from the remote part. As soon
     * as a byte is pushed by the upper model, the byte is sent on the UART interface. It is up
     * to the upper model to check the flow control signal with this method before pushing bytes.
     *
     * @return true if the remote part allows sending bytes.
     */
    bool tx_flow_ctrl_ready();

    /**
     * @brief Send a byte.
     *
     * This is pushing a byte to be sent by the adapter.
     * Only one byte can be pushed at a time.
     * If a byte is pushed while the adapter is not ready, some data will be corrupted.
     * The next byte can be pushed once the last bit of the current byte is sent on the interface.
     * The TX ready event is enqueued when this happens, so that the upper model can push the next
     * byte.
     *
     * @param byte The byte to be sent
     */
    void tx_send_byte(uint8_t byte);

    /**
     * @brief Set user event for sending bytes.
     *
     * This sets the timed event which should be called everytime the adapter becomes ready to
     * send a byte.
     * This happens when the last bit of the current byte has been sent on the interface.
     * The event is enqueued to the time scheduler with 0 delay, which means it behaves as a function
     * call.
     *
     * @param event The user event to be enqueued everytime the adapter becomes ready to send a byte.
     */
    void tx_ready_event_set(vp::TimeEvent *event);

    /**
     * @brief Set user event for TX control flow.
     *
     * This sets the timed event which should be called everytime the TX flow control signal is
     * is changing.
     * This is mostly useful to continue sending after it has been forbidden by remote part.
     * tx_flow_ctrl_ready can be called to know if TX is allowed or not.
     * The event is enqueued to the time scheduler with 0 delay, which means it behaves as a function
     * call.
     *
     * @param event The user event to be enqueued everytime the TX flow control is changing.
     */
    void tx_flow_ctrl_event_set(vp::TimeEvent *event);

private:

    void reset(bool active) override;

    typedef enum
    {
        UART_RX_STATE_WAIT_START,
        UART_RX_STATE_DATA,
        UART_RX_STATE_PARITY,
        UART_RX_STATE_WAIT_STOP
    } uart_rx_state_e;

    typedef enum
    {
        UART_TX_STATE_IDLE,
        UART_TX_STATE_START,
        UART_TX_STATE_DATA,
        UART_TX_STATE_PARITY,
        UART_TX_STATE_STOP
    } uart_tx_state_e;

    static void sync(vp::Block *__this, int data, int clk, int rtr, unsigned int mask);
    void rx_start_sampling(int baudrate);
    static void rx_sample_handler(vp::Block *__this, vp::TimeEvent *event);
    void sample_rx_bit();
    static void tx_send_handler(vp::Block *__this, vp::TimeEvent *event);
    static void tx_init_handler(vp::Block *__this, vp::TimeEvent *event);

    vp::Component *top;
    vp::Trace trace;

    vp::TimeEvent sample_rx_bit_event;
    vp::TimeEvent tx_send_bit_event;
    vp::TimeEvent tx_init_event;

    vp::UartSlave in;

    uint64_t baudrate;
    uint64_t period;
    int data_bits;
    int stop_bits;
    bool parity;
    bool ctrl_flow;
    bool loopback = false;

    uart_rx_state_e rx_state;
    int rx_current_bit;
    int rx_pending_bits;
    uint8_t rx_pending_byte;
    int rx_parity;
    vp::TimeEvent *rx_ready_event = NULL;
    bool cts;

    uart_tx_state_e tx_state;
    int tx_parity;
    int tx_pending_bits;
    uint8_t tx_pending_byte;
    int tx_bit;
    vp::TimeEvent *tx_ready_event = NULL;
    vp::TimeEvent *tx_flow_ctrl_event = NULL;
    bool rts;
};

/**
 * @brief UART buffered adapter
 *
 * This adapter can be used to interact with a UART interface at a higher-level than UartAdapter.
 * This adapter is using the UartAdapter for the low-level UART interaction and adds on top of it
 * a queue of bytes for both TX and RX, so that the upper model does not need to take care about
 * control flow.\n
 * The upper model can for example push as many bytes as needed, and this adapter will take care
 * of sending them when it is possible.\n
 * This model is using a FIFO to store received bytes, which then do not need to be popped
 * immediately, as the FIFO is by default infinite.\n
 * A size can be given to the FIFO to drop any received byte when it is full and easily model
 * a peripheral FIFO.\n
 * A bandwith limit can also be given to let this adapter drives the CTS line to respect the
 * specified number of bytes per second.\n
 * The flow control signal driven by the remote side (RTS) is by default immediately taken into
 * account to stop sending bytes. It is then possible to specify a number of bytes which are still
 * sent after the sending is forbidden to reproduce some behaviors like for FTDI chips.\n
 */
class UartAdapterBuffered : public vp::Block
{
public:
/**
     * @brief Construct a new UART buffered adapter
     *
     * @param top Top component where the UART interface must be created.
     * @param parent Specify the parent block containing this block. This can be the top component
     *        if it is its direct parent.
     * @param name Specify the name of the uart adapter within the parent hierarchy.
     * @param itf_name Specify the name of the UART interface. An interface with this name will be
     *                 created in the top component. This interface must then be connected in
     *                 the python generator with the same name to another UART interface.
     */
    UartAdapterBuffered(vp::Component *top, vp::Block *parent, std::string name, std::string itf_name);

    /**
     * @brief Set the UART baudrate
     *
     * This gives the number of bits per second sent or received.
     * Baudrate is set to 115200 by default.
     *
     * @param baudrate Baudrate in number of bits per second.
     */
    void baudrate_set(int baudrate);

    /**
     * @brief Set the number of data bits
     *
     * This sets the number of data bits exchanged on the interface for each byte exchanged with the
     * upper-level model.
     * It is set to 8 by default and can only be equal or inferior to 8.
     * If it is inferior to 8, the received data bits are shifted right and filled with zero on the
     * left.
     *
     * @param data_bits Number of data bits.
     */
    void data_bits_set(int data_bits);

    /**
     * @brief Set the number of stop bits
     *
     * This sets the number of stop bits exchanged on the interface before switching to the next
     * byte.
     * It is set to 1 by default and can take any value equal or superior to zero.
     *
     * @param stop_bits Number of stop bits.
     */
    void stop_bits_set(int stop_bits);

    /**
     * @brief Set parity enable
     *
     * This enables or disables the parity bit.
     * When enabled, an additional parity bit is transmitted or received right after the data
     * bits to check the data integrity.
     * It is set to false by default.
     * The parity can be retrieved along with the received byte by calling rx_get with the parity
     * argument.
     *
     * @param parity true if parity is enabled.
     */
    void parity_set(bool parity);

    /**
     * @brief Set control flow enable
     *
     * This enables or disables the control flow.
     * The control flow is disabled by default.
     * When disabled, the cts line, is set to 0 to allow receiving
     * bytes. Once control flow is enabled, the line is set again to 0 to wllow receiving bytes,
     * since this adapter has by default an infinite FIFO to store incoming bytes.
     * rx_fifo_size_set or rx_flow_limiter_set can then be called to either limit the FIFO
     * size of the bandwidth and let this adapter drives the cts according to these properties.
     *
     * @param ctrl_flow true if control flow is enabled.
     */
    void ctrl_flow_set(bool ctrl_flow);

    /**
     * @brief Set loopback enable.
     *
     * This enables or disables the loopback.
     * When enabled, received bits are handled as usual but are also immeditely mirrored on the
     * transmit line to model a loopback from RX to TX.
     *
     * @param enabled true if loopback is enabled.
     */
    void loopback_set(bool enabled);

    /**
     * @brief Set user event for receiving bytes.
     *
     * This sets the timed event which should be called everytime a byte has been received and is
     * ready to be retrieved.
     * The event is enqueued to the time scheduler with 0 delay, which means it behaves as a function
     * call.
     * The byte can be retrieved immediately or can be left in the adapter fifo and retrieved later.
     * The timed event is enqueued as soon as the last stop bit is received.
     *
     * @param event The user event to be enqueued everytime a byte is received.
     */
    void rx_ready_event_set(vp::TimeEvent *event);

    /**
     * @brief Tell if RX bytes are ready to be popped.
     *
     * Any received byte is pushed into a FIFO.
     * This function can be called to know if the FIFO contains at least 1 byte which can be popped.
     *
     * @return true if there is at least one byte to pop.
     */
    bool rx_ready();

    /**
      * @brief Poppped a byte from the RX fifo.
      *
      * This should be called only when rx_ready returns true, which means there is at least
      * one byte in the FIFO.
      * The byte is removed from the FIFO and returned.
      *
      * @return The byte which is popped from the FIFO.
      */
    uint8_t rx_pop();

    /**
     * @brief Set the RX FIFO size.
     *
     * This can be used to drive the CTS line according to the state of FIFO.
     * By default the FIFO has an infinite size and the CTS line thus always 0.
     * Once a size is set, the CTS line will be 0 only when the FIFO is not full so that the sender
     * stops sending bytes when the FIFO becomes full
     *
     * @parameter size FIFO size in number of bytes.
     */
    void rx_fifo_size_set(int size);

    /**
     * @brief Set the allowed bandwidth for RX.
     *
     * This can be used to drive the CTS line according to a bandwidth.
     * The model will compute the instant bandwidth and will set CTS line to 0 to allow receiving
     * bytes only if the bandwidth is under what has been specified.
     * In practice, the model will keep switching CTS between 0 and 1 to respect the bandwidth in
     * average, which an easy way to model a UART receiver timing behavior at coarse-grain.
     * This is should not be used at the same time as rx_fifo_size_set as they will both
     * try to driver the CTS.
     *
     * @parameter bandwidth Bandwidth in number of bytes per second.
     */
    void rx_flow_limiter_set(double bandwidth);

    /**
     * @brief Tell if there is no more TX byte on-going.
     *
     * This tells both if the TX FIFO of this adapter is empty and also if the lower-level
     * adapter is not currently sending.
     * This can be used to flush TX.
     *
     * @return true if no TX is on-going.
     */
    bool tx_empty();

    /**
     * @brief Set user event for TX empty.
     *
     * This sets the timed event which should be called everytime the adapter has no more TX byte
     * to send. This also means the lower-level adapter has no TX on-going.
     * This happend when the last bit of the current byte has been sent on the interface.
     * The event is enqueued to the time scheduler with 0 delay, which means it behaves as a function
     * call.
     *
     * @param event The user event to be enqueued everytime the TX is empty.
     */
    void tx_empty_event_set(vp::TimeEvent *event);

    /**
     * @brief Send a byte.
     *
     * This is pushing a byte to be sent by the adapter.
     * AS many bytes as needed can be pushed since the adapter is having an infinte TX FIFO.
     *
     * @param byte The byte to be sent
     */
    void tx_send_byte(uint8_t byte);

    /**
     * @brief Set TX flow control threshold.
     *
     * This sets the number of bytes which can still be sent after the RTS line goes to 1 to
     * disable sending.
     * This can be used to model a behavior where the peripheral does not react immediately to the
     * signal and continues sending for a while.
     * This is the case for FTDI chips where they can still send 4 bytes.
     *
     * @param byte The number of bytes which can still be sent after RTS goes to 1.
     */
    void tx_flow_ctrl_threshold_set(uint8_t byte);

private:
    void reset(bool active) override;

    bool tx_ready();
    static void rx_event_handler(vp::Block *__this, vp::TimeEvent *event);
    static void tx_ready_event_handler(vp::Block *__this, vp::TimeEvent *event);
    static void tx_flow_ctrl_event_handler(vp::Block *__this, vp::TimeEvent *event);
    static void rx_limiter_event_handler(vp::Block *__this, vp::TimeEvent *event);
    void tx_send_byte();
    bool tx_can_send();

    UartAdapter adapter;

    vp::Trace trace;

    int rx_max_pending_bytes = INT_MAX;

    vp::TimeEvent rx_event;
    vp::TimeEvent *rx_event_user = NULL;
    std::queue<uint8_t> rx_queue;
    double ctrl_flow_limiter = 0;
    vp::TimeEvent limiter_event;

    vp::TimeEvent tx_ready_event;
    vp::TimeEvent tx_flow_ctrl_event;
    std::queue<uint8_t> tx_queue;
    int tx_flow_ctrl_threshold;
    int tx_flow_ctrl_threshold_count;
    vp::TimeEvent *tx_empty_event_user = NULL;
};
