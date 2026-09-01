#include "qemu/osdep.h"
#include "hw/usb/usb.h"
#include "desc.h"
#include "qapi/error.h"
#include "migration/vmstate.h"

/* AI Use: Genie gave me a barebones version of this, but it wasn't completely updated for QEMU 11. */
/* I had to walk through that and debug that part.                                                  */

/* This compiles on QEMU emulator version 11.1.50 (v11.1.0-657-gbbc8fb89fa-dirty) */ 

/*

 Add this file to QEMU/hw/usb/my-usb-demo.c and add this line to meson.build: 

+system_ss.add(when: 'CONFIG_USB', if_true: files('my-usb-device.c'))

And these lines to your qemu run script:

  -device qemu-xhci,id=usb \
  -device my-usb-device,bus=usb.0 \

*/ 

#define TYPE_MY_USB "my-usb-device"


/* Device Runtime State Structure */
struct MyUSBState {
    USBDevice dev;
    uint8_t control_reg; /* Custom internal test register */
};


OBJECT_DECLARE_SIMPLE_TYPE(MyUSBState, MY_USB) 


/* String Indices mapping */
enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

/* 3. Updated String Array for QEMU 11 (No USBDescStrings) */
static const char *const desc_strings[] = {
    [STR_MANUFACTURER]     = "Roger D Pease",
    [STR_PRODUCT]          = "Emulated USB Control Device",
    [STR_SERIALNUMBER]     = "12345678",
};

/* 
 * USB is self-describing. When you plug a device in, the driver downloads a bunch of information about what the device can/can't support. 
 * For instance, two printers may have the same vendor/product ID but certain features may not be present. 
 *  
 */ 


/* The hierarchy is Device->Configuratuion->Interface->Endpoint */
/* The data direction is encoded into the Endpoint. 

   Endpoints 0x80 | (4 bits) are IN (to host). 
   Endpoints 0x00 | (4 bits) are OUT (to device). 

   Endpoint 0 of both directions is reserved for control transfers. Bits 6-4 are unused. 
    https://developerhelp.microchip.com/xwiki/bin/view/applications/usb/how-it-works/endpoints/
 
   Let's assume we have a printer/scanner/copier/fax.  
   
   If you could only use:
       The printer and scanner - you can use together or separately  (two interfaces) 
          -OR-
       The copier or fax  (two interfaces with two endpoints) 

   Then you would have separate configurations which would need to be set. 
   In reality if I saw a system like this I'd think it wasn't designed properly anyway. 
  
   For brevity I will just do the print scan side. 

*/ 


/* 
static const USBDescConfig desc_config_copierfax = {
    .bConfigurationValue           = 1,
    .bmAttributes                  = USB_CFG_ATT_ONE,
    .bMaxPower                     = 50,
    .nif                           = 1,
    .ifs                           = &desc_iface_copierfac_copier,
}
*/

/* Scanner side Endpoints */ 
static const USBDescIface desc_iface_printscan_scan = {
    .bInterfaceNumber              = 1, /* 0-255, must be unique across interfaces in the same configuration */ 
    .bNumEndpoints                 = 2, 
    .bInterfaceClass               = USB_CLASS_VENDOR_SPEC,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress      = USB_DIR_IN | 0x01, /* Endpoints must be unique across the same configuration, even in different interfaces. */
                                                        /* IN and OUT are separate endpoints */ 
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 128,
            .bInterval             = 0x05,
        }, {
            .bEndpointAddress      = USB_DIR_OUT | 0x01,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 128,
            .bInterval             = 0x05,
        },
    },
};


static const USBDescIface desc_iface_printscan_print = {
    .bInterfaceNumber              = 0, 
    .bNumEndpoints                 = 2, 
    .bInterfaceClass               = USB_CLASS_VENDOR_SPEC,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress      = USB_DIR_IN | 0x02, 
                                                        
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 128,
            .bInterval             = 0x05,
        }, {
            .bEndpointAddress      = USB_DIR_OUT | 0x02,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 128,
            .bInterval             = 0x05,
        },
    },
};


static const USBDescConfig desc_config_printscan = {
    .bConfigurationValue           = 1,
    .bNumInterfaces                = 2,
    .bmAttributes                  = USB_CFG_ATT_ONE,
    .bMaxPower                     = 50,
    .nif                           = 2,
    .ifs                           = (USBDescIface[]) { desc_iface_printscan_print, desc_iface_printscan_scan } 
};

static const USBDescDevice desc_device_myusb = {
    .bcdUSB                        = 0x0200, /* USB 2.0 device declaration */
    .bDeviceClass                  = USB_CLASS_VENDOR_SPEC,
    .bMaxPacketSize0               = 8,      /* Ep0 packet size limits */
    .bNumConfigurations            = 1,
    .confs                         = (USBDescConfig[]) { desc_config_printscan }, /* If you wanted to do multiple configs you could add them here  */
};


static const USBDesc desc_myusb = {
    .id = {
        .idVendor          = 0xaaaa,
        .idProduct         = 0x0c0c,
        .bcdDevice         = 0x0100,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },

     /* QEMU 11 offers more precise emulation of different speed modes */ 

    .full   = &desc_device_myusb,
    .high   = &desc_device_myusb,
    .str    = desc_strings, 
};


static const VMStateDescription vmstate_my_usb = {
    .name = TYPE_MY_USB,
    .unmigratable = 1, /* Explicitly blocks VM snapshotting/migrations */
};


/* This is for control URBs */ 

static void my_usb_handle_control(USBDevice *dev, USBPacket *p, int request,
                                  int value, int index, int length, uint8_t *data)
{
    printf("my_usb_handle_control \n");
    printf("    Endpoint nr %x\n",p->ep->nr); 
    printf("    request %4X value %d ind %d len %d \n",request,value,index,length); 

    MyUSBState *s = MY_USB(dev);
    int ret;

    /* Process standard USB framework setup packets */
    ret = usb_desc_handle_control(dev, p, request, value, index, length, data);

    if (ret >= 0) {
        printf("Handled by Desc Controller\n"); 
        return;
    }

    /* Process custom vendor setup payloads */
    switch (request) {
      case 0x01: /* Host Write Configuration */
        if (length > 0) {
            s->control_reg = data[0];
            p->actual_length = 1;
        }
        break;
      case 0x02: /* Host Read Configuration */
        if (length > 0) {
            data[0] = s->control_reg;
            p->actual_length = 1;
        }
        break;
    default:
        p->status = USB_RET_STALL;
        break;
    }
    printf("my_usb_handle_control done\n"); 
}


static void my_usb_set_interface(USBDevice *dev, int iface, int old, int value)
{
    printf("USB Set interface\n"); 
}

static void my_usb_unrealize(USBDevice *dev)
{
    printf("my_usb_unrealize called\n"); 
}

static void my_usb_handle_reset(USBDevice *dev)
{
    printf("my_usb_handle_reset called\n"); 

}
static void my_usb_handle_data(USBDevice *dev,USBPacket *packet)
{
    printf("my_usb_handle_data called\n"); 


}

static void my_usb_realize(USBDevice *dev, Error **errp)
{    
    printf("my_usb_realize called\n"); 
    /* Initialize base framework descriptors */
    usb_desc_init(dev);
    printf("my_usb_realize done\n"); 
}

/* 6. Type and Class Configuration Register */
static void my_usb_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->realize        = my_usb_realize;
    uc->handle_reset   = my_usb_handle_reset;
    uc->handle_control = my_usb_handle_control;
    uc->handle_data    = my_usb_handle_data;
    uc->unrealize      = my_usb_unrealize;
    uc->usb_desc       = &desc_myusb;
    uc->product_desc   = "Hi Roger";
    uc->set_interface  = my_usb_set_interface;

    dc->vmsd           = &vmstate_my_usb;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);

    printf("my_usb_class_init done\n"); 
}

static const TypeInfo my_usb_info = {
    .name          = TYPE_MY_USB,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(MyUSBState),
    .class_init    = my_usb_class_init,
};

static void my_usb_register_types(void)
{
    type_register_static(&my_usb_info);
}

type_init(my_usb_register_types)

