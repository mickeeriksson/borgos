#ifndef _DRIVER_USB_USB_H_
#define _DRIVER_USB_USB_H_

#include "driver/usb/usb_std.h"

#define USB_DEBUG 1

#define USB_MIN_LOG_PRIORITY        4
#define USB_LOG_PRIORITY_ERROR      3
#define USB_LOG_PRIORITY_INFO       2
#define USB_LOG_PRIORITY_DEBUG      1

#define USB_MAXSTRINGLEN 128
/** Maximum number of interfaces per device.  */
#define USB_DEVICE_MAX_INTERFACES 8

/** Maximum number of endpoints per interface.  */
#define USB_DEVICE_MAX_ENDPOINTS  8

enum kUSB_speed {
    kUSB_SPEED_UNKNOWN  = 0,
    kUSB_SPEED_LOW  = 1,
    kUSB_SPEED_FULL = 2,
//    USB_SPEED_HIGH = 0,
};

typedef int USBRESULT;
#define USB_OK 0
#define USB_ERROR                      -1
#define USB_STATUS_INVALID_DATA        1002
#define USB_STATUS_OUT_OF_MEMORY       1003
#define USB_STATUS_DEVICE_UNSUPPORTED  1004

#define USB_XFERFLAG_ASYNC 0x01;

struct usb_device;
struct usb_device_driver {
    // Name of the driver, for informational messages only.  This can be @c NULL.
    const char *name;

    USBRESULT (*probe)(struct usb_device *dev);
    USBRESULT (*bind_device)(struct usb_device *dev);
    void (*unbind_device)(struct usb_device *dev);
};

//Driver Annotation
typedef struct {
    const char *name;
    void* driver;
} usbdriver_annotation_t;

// Makro för att förenkla annoteringen och tvinga kompilatorn att behålla koden
#define REGISTER_USB_DRIVER(obj_name, mod_name,driver_ptr) \
static const usbdriver_annotation_t obj_name __attribute__((section("usbdriver_annotation"), used)) = { \
.name = mod_name, \
.driver = driver_ptr, \
}



//this is where all controller related info is stored.
//One struct per controller independent of implementations
struct usb_hcd;
struct usb_xfer_request;

enum hcd_state {
    HCD_STATE_UNDEFINED = 0,
    HCD_STATE_INIT      = 1,      //after attached or released
    HCD_STATE_SETUP     = 2,      //after successfull setup, or stop
    HCD_STATE_STARTED   = 3,      //after successfull start
};

typedef struct usb_hcd {
    const char *name;
    //list of attached usb-devices
    //how to implement?
    int hcdstate;

    //operation pointers
    USBRESULT (*setup)(struct usb_hcd *hcd);    //allocate structures, and init controller
    USBRESULT (*release)(struct usb_hcd *hcd);  //stop controller and relerase structures.
    USBRESULT (*start)(struct usb_hcd *hcd);    //start schedule, start irq
    USBRESULT (*stop)(struct usb_hcd *hcd);     //stop schedule stop irq
    USBRESULT (*submit_ctrl_xfer_request)(struct usb_xfer_request *req,int waitforcompletion,int timeout);
    USBRESULT (*submit_endp_xfer_request_sync)(struct usb_xfer_request *req,int waitforcompletion,int timeout);
    USBRESULT (*submit_endp_xfer_request)(struct usb_xfer_request *req);
    USBRESULT (*poll)(struct usb_hcd *hcd);

    //TEMP, replace by virtual roothub later on
    BOOL (*hcd_roothub_is_port_present)(struct usb_hcd *hcd, uint8_t portidx);
    USBRESULT (*hcd_roothub_port_reset)(struct usb_hcd *hcd, uint8_t portidx);
    BOOL (*hcd_roothub_is_port_device_present)(struct usb_hcd *hcd, uint8_t portidx);
    int (*hcd_roothub_port_speed)(struct usb_hcd *hcd, uint8_t portidx);


} usb_hcd_t;

typedef struct usb_device{
    //ALOT OF This struct is influenced bu XINU

    //host controller for this dev
    usb_hcd_t *usb_hcd;

    //  Address of this device.  Set by USB core.
    uint8_t address;





    //Speed at which this device is attached to its hub.  Set by USB core.
    enum usb_speed speed;


    //Current configuration index of this USB device.  Set by USB core.
    uint8_t configuration;

    struct USB20_device_descriptor descriptor;

    struct USB20_configuration_descriptor *config_descriptor;


    //Pointers to all interface descriptors of this device.  Set by USB core.
    struct USB20_interface_descriptor *interfaces[USB_DEVICE_MAX_INTERFACES];

    //Pointers to all endpoint descriptors of this device, arranged by interface.  Set by USB core.
    struct USB20_endpoint_descriptor *endpoints[USB_DEVICE_MAX_INTERFACES][USB_DEVICE_MAX_ENDPOINTS];

    // Null-terminated product string (ASCII encoded, and in English if available) of this device.
    // Will be the empty string if the device has no product string or it could not be read.
    // Set by USB core.
    char *product;

    // Null-terminated manufacturer string (ASCII encoded, and in English if available) of this device.
    // Will be the empty string if the device has no anufacturer string or it could not be read.
    // Set by USB core.
    char *manufacturer;


    // Private data for the driver of this USB device.  USB device drivers can
    // * place a pointer to a private data structure here in their @ref
    // * usb_device_driver::bind_device "bind_device" routines.
    void *priv;

    //  Driver bound to this device, if any.  Set by USB core.
    struct usb_device_driver *driver;
} usb_device_t;


typedef struct usb_xfer_request {

    // USB device to communicate with.
    struct usb_device *dev;


     // USB_XFERFLAG_ASYNC = AsyncMode, ie call to submit_endp_xfer_request does not wait for completion
    uint32_t xferFlags ;

   // Pointer to the descriptor of the endpoint to communicate with.  You can
    // get a pointer to one of these by searching the endpoints array of the
    // struct usb_device.  Use @c NULL to specify the default control endpoint.
    const struct USB20_endpoint_descriptor *endpoint_desc;


    // Data buffer: whether the sendbuf or recvbuf is used depends on whether
    // * the endpoint_number specifies an IN or OUT endpoint.
    //    union {
    //         Buffer of data to send.  Ignored if size is 0.
    //        void *sendbuf;
    //        Buffer into which to write the received data.  Ignored if size is 0.
    //
    //        void *recvbuf;
    //    };
    void *xferbuf;

    // Size of sendbuf or recvbuf.  For IN endpoints, this will be the maximum
    // * number of bytes of data to receive.  For OUT endpoints, this will be the
    // * exact number of bytes of data to send.
    uint16_t xferlen;

    //  Setup data for the USB control request.  Must be filled in for control
    // * transfers; ignored otherwise.  Note: consider using usb_control_msg() for
    // * control transfers instead.
    //struct usb_control_setup_data setup_data;
    struct USB20_control_setup_data  *setup_data;

    //  Callback function that will be called when this USB transfer has been
    // * successfully completed or has failed.
    void (*xfer_cb_func)(struct usb_xfer_request *req);

    // * USB device driver private data to be saved for the completion callback.
    // * Setting this is optional.
    void *priv;

    // status of xfer USB_OK if complete and sucess...
    USBRESULT status;

    uint16_t actual_xferlen;
}usb_xfer_request_t;



extern void usb_init(void);
extern void usb_attach_hcd(usb_hcd_t *hcd) ;

extern void usb_poll(void) ;
//extern USBRESULT usb_attach_device(struct usb_device *dev);
//extern struct usb_device* usb_alloc_device(void);
//extern void usb_free_device(struct usb_device* dev);
//extern void usb_register_driver(struct usb_device_driver *driver);
USBRESULT usb_submit_xfer_request(struct usb_xfer_request *req);
extern USBRESULT usb_control_msg(struct usb_device *dev,
                          const struct USB20_endpoint_descriptor *endpoint_desc,
                          uint8_t bRequest, uint8_t bmRequestType,
                          uint16_t wValue, uint16_t wIndex, void *data, uint16_t wLength);

extern struct usb_xfer_request* usb_alloc_xfer_request(uint16_t bufsize);
extern void usb_free_xfer_request(struct usb_xfer_request* req);


//DEBUG
#ifdef USB_DEBUG
void usb_register_driver(struct usb_device_driver *driver);
extern void usb_debug_device_info(struct usb_device* dev);
extern const char *usb_device_description(const struct usb_device *dev);
extern void usb_log(int priority, const char *func,struct usb_device *dev, const char *format, ...);
extern const char *usb_transfer_type_to_string(enum usb_transfer_type type);
extern const char *usb_direction_to_string(enum usb_direction dir) ;
#endif




#ifdef USB_DEBUG
#define usb_dev_error(dev,format, ...) usb_log(USB_LOG_PRIORITY_ERROR, __func__, dev, format, ##__VA_ARGS__)
#define usb_dev_info(dev,format, ...)  usb_log(USB_LOG_PRIORITY_INFO, __func__, dev, format, ##__VA_ARGS__)
#define usb_dev_debug(dev,format, ...) usb_log(USB_LOG_PRIORITY_DEBUG, __func__, dev, format, ##__VA_ARGS__)
#define usb_dev_debug2(dev,format, ...) usb_log(USB_LOG_PRIORITY_DEBUG, __func__, dev, format, ##__VA_ARGS__)
#define usb_dev_debug3(dev,format, ...) usb_log(USB_LOG_PRIORITY_DEBUG, __func__, dev, format, ##__VA_ARGS__)     //HCD_level
//#define usb_dev_debug3(dev,format, ...)                                                                             //HCD_level
#define usb_error(format, ...) usb_log(USB_LOG_PRIORITY_ERROR, __func__, NULL, format, ##__VA_ARGS__)
#define usb_info(format, ...)  usb_log(USB_LOG_PRIORITY_INFO, __func__, NULL, format, ##__VA_ARGS__)
#define usb_debug(format, ...) usb_log(USB_LOG_PRIORITY_DEBUG, __func__, NULL, format, ##__VA_ARGS__)
#define usb_debug2(format, ...) usb_log(USB_LOG_PRIORITY_DEBUG, __func__, NULL, format, ##__VA_ARGS__)
//#define usb_debug3(format, ...) usb_log(USB_LOG_PRIORITY_DEBUG, __func__, NULL, format, ##__VA_ARGS__)            //HCD_level
#define usb_debug3(format, ...)                                                                                    //HCD_level

#else
#define usb_dev_error(dev,format, ...)
#define usb_dev_info(dev,format, ...)
#define usb_dev_debug(dev,format, ...)
#define usb_error(format, ...)
#define usb_info(format, ...)
#define usb_debug(format, ...)
#endif

#endif