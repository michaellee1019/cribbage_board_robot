#include <UsbConnection.hpp>

#include <Arduino.h>
#include <hal/usb_serial_jtag_ll.h>
#include <soc/usb_serial_jtag_struct.h>

void UsbConnectionMonitor::begin() {
    // HWCDC does not consume the SOF interrupt in the pinned Arduino core, so
    // this monitor can use its raw status bit without opening the serial port.
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SOF);
}

bool UsbConnectionMonitor::poll() {
    return state_.observe(sampleStartOfFrame());
}

bool UsbConnectionMonitor::connected() const {
    return state_.connected();
}

bool UsbConnectionMonitor::connectionAppearsWithin(uint32_t windowMs) {
    if (connected()) {
        return true;
    }

    const uint32_t startedMs = millis();
    do {
        if (poll()) {
            return true;
        }
        delay(1);
    } while (static_cast<uint32_t>(millis() - startedMs) < windowMs);

    return false;
}

bool UsbConnectionMonitor::sampleStartOfFrame() {
    const bool received = USB_SERIAL_JTAG.int_raw.sof_int_raw != 0;
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SOF);
    return received;
}
