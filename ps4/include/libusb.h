#pragma once
// OpenOrbis v0.5.4 orbis/Usbd.h requests <libusb.h>, while the actual
// bundled declaration is <orbis/_types/libusb.h>. This redirect uses the
// SDK's header; it does not substitute a host libusb implementation.
#include <orbis/_types/libusb.h>
