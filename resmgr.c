#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/netmgr.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <devctl.h>

#include <sys/dispatch.h>

#include <rpi4bt/rpi4bt_msg.h>

void ctrl_process_command(const int command, uint8_t *in, uint8_t *out, int *nbytes);
// <- Since we are overritting "iofunc_attr_t", the following declaration should set before including "sys/iofunc.h"
struct TPattr_s;
#define IOFUNC_ATTR_T   struct TPattr_s
#include <sys/iofunc.h>
// ->

static resmgr_connect_funcs_t connect_funcs;
static resmgr_io_funcs_t io_funcs;


#define NUM_DEVICES    3


typedef struct TPattr_s {
    iofunc_attr_t   attr;
    int             device;     // one of TP_FAST, TP_MEDIUM, TP_SLOW (see below)
} TPattr_t;

//  device information table
TPattr_t  tpattrs [NUM_DEVICES];

char    *devnames [NUM_DEVICES] =
{
    "/dev/rpiSPP",
    "/dev/rpiHID",
    "/dev/rpiCTRL"
};

#define RPI_SPP        0
#define RPI_HID        1
#define RPI_CTRL       2

// Function Prototypes
int io_read(resmgr_context_t *ctp, io_read_t *msg, iofunc_ocb_t *ocb);
int io_write(resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb);
int io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle, void *extra);
int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, iofunc_ocb_t *ocb);

int setup_resource_manager() 
{
    thread_pool_attr_t pool_attr;
    resmgr_attr_t resmgr_attr;
    dispatch_t *dpp;
    thread_pool_t *tpp;
    int id;

    pthread_setname_np(pthread_self(), "BT ResMgr");

    if((dpp = dispatch_create()) == NULL) {
        fprintf(stderr, "Unable to allocate dispatch handle.\n");
        return EXIT_FAILURE;
    }

    memset(&resmgr_attr, 0, sizeof(resmgr_attr));
    resmgr_attr.nparts_max = 4;
    resmgr_attr.msg_max_size = 16384;

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, 
                     _RESMGR_IO_NFUNCS, &io_funcs);

    // Set up the IO functions
    io_funcs.read = io_read;
    io_funcs.write = io_write;
    connect_funcs.open = io_open;
    io_funcs.devctl = io_devctl;

    for (int i = 0; i < NUM_DEVICES; i++) {

        iofunc_attr_init (&tpattrs [i].attr, S_IFCHR | 0666, NULL, NULL);
        tpattrs [i].device = i;              
        int ret = resmgr_attach (dpp, NULL, devnames [i],
                    _FTYPE_ANY, 0, &connect_funcs, &io_funcs, &tpattrs [i]);

        if (ret == -1) {
            fprintf (stderr, "couldn't attach pathname %s, errno %d\n", devnames [i], errno);
            exit (EXIT_FAILURE);
        }
    }

    memset(&pool_attr, 0, sizeof(pool_attr));
    pool_attr.handle = dpp;
    pool_attr.context_alloc = dispatch_context_alloc;
    pool_attr.block_func = dispatch_block;
    pool_attr.unblock_func = dispatch_unblock;
    pool_attr.handler_func = dispatch_handler;
    pool_attr.context_free = dispatch_context_free;
    pool_attr.lo_water = 2;
    pool_attr.hi_water = 4;
    pool_attr.increment = 1;
    pool_attr.maximum = 50;

    if((tpp = thread_pool_create(&pool_attr, POOL_FLAG_EXIT_SELF)) == NULL) {
        fprintf(stderr, "Unable to initialize thread pool.\n");
        return EXIT_FAILURE;
    }

    thread_pool_start(tpp);
    return EXIT_SUCCESS; // This line is theoretical, as thread_pool_start does not return.
}

// Define the IO functions
int io_read(resmgr_context_t *ctp, io_read_t *msg, iofunc_ocb_t *ocb) 
{
    char data[1024];
    int nbytes = 0;
    int status;

    // extra sanity check
    if ((status = iofunc_read_verify(ctp, msg, ocb, NULL)) != EOK)
        return status;
        
    // No special xtypes
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) {
        return ENOSYS;
    }


    /* figure out device based on ocb */
    int device = ocb->attr->device;

    switch (device) {
        case RPI_SPP:
        {
            if (!bt_client_data_available())
                return EAGAIN; // If no data is available, inform the client

            nbytes = bt_client_read(data, msg->i.nbytes);
            break;
        }
        case RPI_HID:
        {
            if ((nbytes = circular_buffer_read(data)) == 0){
                //printf("circular_buffer_empty EAGAIN\n");
                return EAGAIN; // If no data is available, inform the client
            }
            break;
        }
        case RPI_CTRL:
        {
            printf("TODO: RPI_CTRL io_read\n");
            exit(0);
            break;
        }
        default:
        {
            printf("Invalid device\n");
            MsgError(ctp->rcvid, -1);
            return _RESMGR_NOREPLY;
        }
    }

    MsgReply(ctp->rcvid, nbytes, data, nbytes);

    return _RESMGR_NOREPLY;
}

int io_write(resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb) 
{
    char buf[1024]; // FIXME: why 1024?
    int status;
    int nbytes;

    /* figure out device based on ocb */
    int device = ocb->attr->device;
    switch (device) {
        case RPI_SPP:
        {
            status = resmgr_msgread(ctp, buf, msg->i.nbytes, sizeof(msg->i));
            if (status < 0)  {
                printf("Error: io_write\n");
                return status;
            }
            // display_bytes("io_write: ", buf, msg->i.nbytes);
            nbytes = bt_client_write(buf, msg->i.nbytes);
            break;
        }
        case RPI_HID:
        {
            printf("TODO: Got Write from RPI_HID\n");
            exit(0);
            break;
        }
        case RPI_CTRL:
        {
            printf("TODO: RPI_CTRL io_write\n");
            exit(0);
            break;
        }
        default:
        {
            printf("Invalid device\n");
            MsgError(ctp->rcvid, -1);
            return _RESMGR_NOREPLY;
        }

    }

    MsgReply(ctp->rcvid, status, NULL, 0);

    return _RESMGR_NOREPLY;
}

int io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle, void *extra) {
    printf("Device opened\n");
    return iofunc_open_default(ctp, msg, handle, extra);
}

int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, iofunc_ocb_t *ocb) 
{
    int status = iofunc_devctl_default(ctp, msg, ocb);
    if (status != _RESMGR_DEFAULT) {
        return status;
    }


    /* figure out device based on ocb */
    int device = ocb->attr->device;
    switch (device) {
        case RPI_SPP:
        {
            printf("Invalid device\n");
            MsgError(ctp->rcvid, -1);
            return _RESMGR_NOREPLY;
        }
        case RPI_HID:
        {
            printf("FIXME: Got io_devctl from RPI_HID\n");
            //exit(0);
            break;
        }
        case RPI_CTRL:
        {
            if (msg->i.dcmd == RPI4_CUSTOM_COMMAND) {
                int nbytes, status;

                char *output_buf = malloc(32768);
                if(!output_buf) {
                    printf("malloc() failed\n");
                    exit(0);
                }
                // Use MsgRead() if received mesg is big and can't fit cache size???
                custom_msg_t *o_msg = _DEVCTL_DATA(msg->o);

                // Don't clear o_msg since it points the input mesg (msg->i) which has the cmd value
                // memset(o_msg, 0, sizeof(custom_msg_t));
                //nbytes = devctl_process_command(ctp, o_msg, output_buf);
                status = devctl_process_command(ctp, msg);
                if (status == -1) {
                    printf("devctl_process_command() failed\n");
                    MsgError(ctp->rcvid, -1);
                    // FIXME: do what???
                }

/*
                // Why do we need to add "sizeof(msg->o)" when replying???
                SETIOV(ctp->iov + 0, &msg->o, sizeof(msg->o) + sizeof(custom_msg_t));
                SETIOV(ctp->iov + 1, output_buf, 4096);
                msg->o.nbytes = nbytes;
printf("-- MsgReplyv failed - ctp->rcvid:%d\n", ctp->rcvid);
                status = MsgReplyv(ctp->rcvid, EOK, ctp->iov, 2);
                if (status == -1) {
                    perror("MsgReplyv failed");
                }
*/
                return _RESMGR_NOREPLY;
            }
            break;
        }
        default:
        {
            printf("Invalid device\n");
            MsgError(ctp->rcvid, -1);
            return _RESMGR_NOREPLY;
        }

    }

    MsgReply(ctp->rcvid, status, NULL, 0);
    return _RESMGR_NOREPLY;
}
