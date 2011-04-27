#ifndef __DRIVER_H__
#define __DRIVER_H__

/**
 * Levels of verbosity to implement.
 */
typedef enum _VERBOSITY
{
  /** No messages are printed */	
  GLEVEL0,
  
  /** Few messages are printed */
  GLEVEL1,
  
  /** More messages are printed */
  GLEVEL2
  
  	
} VERBOSITY;

typedef enum _GADGET_EXIT_CODE
{
  /** Everything OK */	
  GAD_EOK,	
  
  /** Error changing to gadget directory */
  ERR_GAD_DIR,
  
  /** Error creating thread */
  ERR_THRD,
  
  /** Error joining thread */
  ERR_JN_THRD,
  
  /** Error opening file descriptor */
  ERR_OPEN_FD,
  
  /** Error writing to file descriptor */
  ERR_WRITE_FD,
  
  /** File descriptor short write */
  SHORT_WRITE_FD,
  
  /** No device to configure */
  ERR_NO_DEVICE,
  	
} GADGET_EXIT_CODE;

/**
 * Endpoint utilities
 */
typedef struct _endpoint
{
	/** Endpoint thread */
	pthread_t thread;
	
	/** Endpoint file descriptor */
	int fd;
	
	/** Endpoint name */
	char *NAME;
	
	/** Endpoint related function */
	void *(*func) (void *);
	
} endpoint;

/**
 * Gadget struct
 */
typedef struct _usb_gadget
{
  /** Endpoint0 structure */
  endpoint ep0;
  
  /** Streaming enpoint structure */
  endpoint stream;
  
  /** Downstream events structure */
  endpoint ev_down;
  
  /** Upstream events structure */
  endpoint ev_up;
  
  /** Device name */
  char *DEVNAME;
  
  /** Level of verbosity of the execution */
  VERBOSITY verbosity;
  
} usb_gadget;

/**
  * \brief Object constructor.
  * \param gadget Object to create.
  * \param v Verbosity level of the context. See #_VERBOSE.
  * \return Code with the return status.
  */
extern GADGET_EXIT_CODE usb_gadget_new(usb_gadget *gadget, VERBOSITY v);

extern GADGET_EXIT_CODE usb_gadget_free(usb_gadget *gadget);


#endif /* __DRIVER_H__ */
