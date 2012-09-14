/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/* server-transform.c:  an example program that sends response content
 *                      to a server to be transformed, and sends the
 *                      transformed content to the client
 *
 *
 *	Usage:
 *	(NT): ServerTransform.dll
 *	(Solaris): server-transform.so
 *
 *
 */

/* The protocol spoken with the server is simple. The plugin sends the
   content-length of the document being transformed as a 4-byte
   integer and then it sends the document itself. The first 4-bytes of
   the server response are a status code/content length. If the code
   is greater than 0 then the plugin assumes transformation was
   successful and uses the code as the content length of the
   transformed document. If the status code is less than or equal to 0
   then the plugin bypasses transformation and sends the original
   document on through.

   The plugin does a fair amount of error checking and tries to bypass
   transformation in many cases such as when it can't connect to the
   server. This example plugin simply connects to port 7 on localhost,
   which on our solaris machines (and most unix machines) is the echo
   port. One nicety about the protocol is that simply having the
   server echo back what it is sent results in a "null"
   transformation. (i.e. A transformation which does not modify the
   content). */

#include <string.h>
#include <stdio.h>

#if !defined (_WIN32)
#  include <netinet/in.h>
#else
#  include <windows.h>
#endif

#include <ts/ts.h>

#define STATE_BUFFER       1
#define STATE_CONNECT      2
#define STATE_WRITE        3
#define STATE_READ_STATUS  4
#define STATE_READ         5
#define STATE_BYPASS       6

typedef struct
{
  int state;
  TSHttpTxn txn;

  TSIOBuffer input_buf;
  TSIOBufferReader input_reader;

  TSIOBuffer output_buf;
  TSIOBufferReader output_reader;
  TSVConn output_vc;
  TSVIO output_vio;

  TSAction pending_action;
  TSVConn server_vc;
  TSVIO server_vio;

  int content_length;
} TransformData;

static TSCont transform_create(TSHttpTxn txnp);
static void transform_destroy(TSCont contp);
static int transform_connect(TSCont contp, TransformData * data);
static int transform_write(TSCont contp, TransformData * data);
static int transform_read_status(TSCont contp, TransformData * data);
static int transform_read(TSCont contp, TransformData * data);
static int transform_bypass(TSCont contp, TransformData * data);
static int transform_buffer_event(TSCont contp, TransformData * data, TSEvent event, void *edata);
static int transform_connect_event(TSCont contp, TransformData * data, TSEvent event, void *edata);
static int transform_write_event(TSCont contp, TransformData * data, TSEvent event, void *edata);
static int transform_read_status_event(TSCont contp, TransformData * data, TSEvent event, void *edata);
static int transform_read_event(TSCont contp, TransformData * data, TSEvent event, void *edata);
static int transform_bypass_event(TSCont contp, TransformData * data, TSEvent event, void *edata);
static int transform_handler(TSCont contp, TSEvent event, void *edata);

#if !defined (_WIN32)
static in_addr_t server_ip;
#else
static unsigned int server_ip;
#endif

static int server_port;

static TSCont
transform_create(TSHttpTxn txnp)
{
  TSCont contp;
  TransformData *data;

  if ((contp = TSTransformCreate(transform_handler, txnp)) == TS_ERROR_PTR) {
    TSError("Error in creating Transformation. Retyring...");
    return NULL;
  }

  data = (TransformData *) TSmalloc(sizeof(TransformData));
  data->state = STATE_BUFFER;
  data->txn = txnp;
  data->input_buf = NULL;
  data->input_reader = NULL;
  data->output_buf = NULL;
  data->output_reader = NULL;
  data->output_vio = NULL;
  data->output_vc = NULL;
  data->pending_action = NULL;
  data->server_vc = NULL;
  data->server_vio = NULL;
  data->content_length = 0;

  if (TSContDataSet(contp, data) != TS_SUCCESS) {
    TSError("Error in setting continuation's data. TSContDataSet doesn't return TS_SUCCESS");
  }

  return contp;
}

static void
transform_destroy(TSCont contp)
{
  TransformData *data;

  data = TSContDataGet(contp);
  if ((data != TS_ERROR_PTR) || (data != NULL)) {
    if (data->input_buf) {
      if (TSIOBufferDestroy(data->input_buf) != TS_SUCCESS) {
        TSError("Unable to destroy input IO buffer");
      }
    }

    if (data->output_buf) {
      if (TSIOBufferDestroy(data->output_buf) != TS_SUCCESS) {
        TSError("Unable to destroy output IO buffer");
      }
    }

    if (data->pending_action) {
      if (TSActionCancel(data->pending_action) != TS_SUCCESS) {
        TSError("Unable to cancel the pending action");
      }
    }

    if (data->server_vc) {
      if (TSVConnAbort(data->server_vc, 1) != TS_SUCCESS) {
        TSError("Unable to abort server VConnection. TSVConnAbort doesn't return TS_SUCESS");
      }
    }

    TSfree(data);
  } else {
    TSError("Unable to get Continuation's Data. TSContDataGet returns TS_ERROR_PTR or NULL");
  }

  if (TSContDestroy(contp) != TS_SUCCESS) {
    TSError("Error in Destroying the continuation");
  }
}

static int
transform_connect(TSCont contp, TransformData * data)
{
  TSAction action;
  int content_length;

  data->state = STATE_CONNECT;

  content_length = TSIOBufferReaderAvail(data->input_reader);
  if (content_length != TS_ERROR) {
    data->content_length = content_length;
    data->content_length = htonl(data->content_length);

    /* Prepend the content length to the buffer.
     * If we decide to not send the content to the transforming
     * server then we need to make sure and skip input_reader
     * over the content length.
     */

    {
      TSIOBuffer temp;
      TSIOBufferReader tempReader;

      temp = TSIOBufferCreate();
      if (temp != TS_ERROR_PTR) {
        tempReader = TSIOBufferReaderAlloc(temp);

        if (tempReader != TS_ERROR_PTR) {

          if (TSIOBufferWrite(temp, (const char *) &data->content_length, sizeof(int)) == TS_ERROR) {
            TSError("TSIOBufferWrite returns TS_ERROR");
            if (TSIOBufferReaderFree(tempReader) == TS_ERROR) {
              TSError("TSIOBufferReaderFree returns TS_ERROR");
            }
            if (TSIOBufferDestroy(temp) == TS_ERROR) {
              TSError("TSIOBufferDestroy returns TS_ERROR");
            }
            return 0;
          }

          if (TSIOBufferCopy(temp, data->input_reader, data->content_length, 0) == TS_ERROR) {
            TSError("TSIOBufferCopy returns TS_ERROR");
            if (TSIOBufferReaderFree(tempReader) == TS_ERROR) {
              TSError("TSIOBufferReaderFree returns TS_ERROR");
            }
            if (TSIOBufferDestroy(temp) == TS_ERROR) {
              TSError("TSIOBufferDestroy returns TS_ERROR");
            }
            return 0;
          }

          if (TSIOBufferReaderFree(data->input_reader) == TS_ERROR) {
            TSError("Unable to free IOBuffer Reader");
          }

          if (TSIOBufferDestroy(data->input_buf) == TS_ERROR) {
            TSError("Trying to destroy IOBuffer returns TS_ERROR");
          }

          data->input_buf = temp;
          data->input_reader = tempReader;

        } else {
          TSError("Unable to allocate a reader for buffer");
          if (TSIOBufferDestroy(temp) == TS_ERROR) {
            TSError("Unable to destroy IOBuffer");
          }
          return 0;
        }
      } else {
        TSError("Unable to create IOBuffer.");
        return 0;
      }
    }
  } else {
    TSError("TSIOBufferReaderAvail returns TS_ERROR");
    return 0;
  }

  action = TSNetConnect(contp, server_ip, server_port);
  if (action != TS_ERROR_PTR) {
    if (!TSActionDone(action)) {
      data->pending_action = action;
    }
  } else {
    TSError("Unable to connect to server. TSNetConnect returns TS_ERROR_PTR");
  }

  return 0;
}

static int
transform_write(TSCont contp, TransformData * data)
{
  int content_length;

  data->state = STATE_WRITE;

  content_length = TSIOBufferReaderAvail(data->input_reader);
  if (content_length != TS_ERROR) {

    data->server_vio =
      TSVConnWrite(data->server_vc, contp, TSIOBufferReaderClone(data->input_reader), content_length);
    if (data->server_vio == TS_ERROR_PTR) {
      TSError("TSVConnWrite returns TS_ERROR_PTR");
    }
  } else {
    TSError("TSIOBufferReaderAvail returns TS_ERROR");
  }
  return 0;
}

static int
transform_read_status(TSCont contp, TransformData * data)
{
  data->state = STATE_READ_STATUS;

  data->output_buf = TSIOBufferCreate();
  if ((data->output_buf != NULL) && (data->output_buf != TS_ERROR_PTR)) {
    data->output_reader = TSIOBufferReaderAlloc(data->output_buf);
    if ((data->output_reader != NULL) && (data->output_reader != TS_ERROR_PTR)) {
      data->server_vio = TSVConnRead(data->server_vc, contp, data->output_buf, sizeof(int));
      if (data->server_vio == TS_ERROR_PTR) {
        TSError("TSVConnRead returns TS_ERROR_PTR");
      }

    } else {
      TSError("Error in Allocating a Reader to output buffer. TSIOBufferReaderAlloc returns NULL or TS_ERROR_PTR");
    }
  } else {
    TSError("Error in creating output buffer. TSIOBufferCreate returns TS_ERROR_PTR");
  }
  return 0;
}

static int
transform_read(TSCont contp, TransformData * data)
{
  data->state = STATE_READ;

  if (TSIOBufferDestroy(data->input_buf) != TS_SUCCESS) {
    TSError("Unable to destroy input IO Buffer. TSIOBuffer doesn't return TS_SUCCESS");
  }
  data->input_buf = NULL;
  data->input_reader = NULL;

  data->server_vio = TSVConnRead(data->server_vc, contp, data->output_buf, data->content_length);

  if (data->server_vio == TS_ERROR_PTR) {
    TSError("TSVConnRead returns TS_ERROR_PTR");
    return -1;
  }

  data->output_vc = TSTransformOutputVConnGet((TSVConn) contp);
  if ((data->output_vc == TS_ERROR_PTR) || (data->output_vc == NULL)) {
    TSError("TSTransformOutputVConnGet returns NULL or TS_ERROR_PTR");
  } else {
    data->output_vio = TSVConnWrite(data->output_vc, contp, data->output_reader, data->content_length);
    if ((data->output_vio == TS_ERROR_PTR) || (data->output_vio == NULL)) {
      TSError("TSVConnWrite returns NULL or TS_ERROR_PTR");
    }
  }

  return 0;
}

static int
transform_bypass(TSCont contp, TransformData * data)
{
  data->state = STATE_BYPASS;

  if (data->server_vc) {
    if (TSVConnAbort(data->server_vc, 1) != TS_SUCCESS) {
      TSError("Error in destroy server vc. TSVConnAbort doesn't return TS_SUCCESS");
    }
    data->server_vc = NULL;
    data->server_vio = NULL;
  }

  if (data->output_buf) {
    if (TSIOBufferDestroy(data->output_buf) != TS_SUCCESS) {
      TSError("Error in destroy output IO buffer. TSIOBufferDestroy doesn't return TS_SUCCESS");
    }
    data->output_buf = NULL;
    data->output_reader = NULL;
  }

  if (TSIOBufferReaderConsume(data->input_reader, sizeof(int)) != TS_SUCCESS) {
    TSError("Error in Consuming bytes from Reader. TSIObufferReaderConsume doesn't return TS_SUCCESS");
  }

  data->output_vc = TSTransformOutputVConnGet((TSVConn) contp);
  if ((data->output_vc == TS_ERROR_PTR) || (data->output_vc == NULL)) {
    TSError("TSTransformOutputVConnGet returns NULL or TS_ERROR_PTR");
  } else {
    data->output_vio =
      TSVConnWrite(data->output_vc, contp, data->input_reader, TSIOBufferReaderAvail(data->input_reader));
    if ((data->output_vio == TS_ERROR_PTR) || (data->output_vio == NULL)) {
      TSError("TSVConnWrite returns NULL or TS_ERROR_PTR");
    }
  }
  return 1;
}

static int
transform_buffer_event(TSCont contp, TransformData * data, TSEvent event, void *edata)
{
  TSVIO write_vio;
  int towrite;
  int avail;

  if (!data->input_buf) {
    data->input_buf = TSIOBufferCreate();
    if ((data->input_buf == NULL) || (data->input_buf == TS_ERROR_PTR)) {
      TSError("Error in Creating buffer");
      return -1;
    }
    data->input_reader = TSIOBufferReaderAlloc(data->input_buf);
    if ((data->input_reader == NULL) || (data->input_reader == TS_ERROR_PTR)) {
      TSError("Unable to allocate a reader to input buffer.");
      return -1;
    }
  }

  /* Get the write VIO for the write operation that was performed on
     ourself. This VIO contains the buffer that we are to read from
     as well as the continuation we are to call when the buffer is
     empty. */
  write_vio = TSVConnWriteVIOGet(contp);
  if (write_vio == TS_ERROR_PTR) {
    TSError("Corrupted write VIO received.");
  }

  /* We also check to see if the write VIO's buffer is non-NULL. A
     NULL buffer indicates that the write operation has been
     shutdown and that the continuation does not want us to send any
     more WRITE_READY or WRITE_COMPLETE events. For this buffered
     transformation that means we're done buffering data. */
  if (!TSVIOBufferGet(write_vio)) {
    return transform_connect(contp, data);
  }

  /* Determine how much data we have left to read. For this server
     transform plugin this is also the amount of data we have left
     to write to the output connection. */
  towrite = TSVIONTodoGet(write_vio);
  if (towrite > 0) {
    /* The amount of data left to read needs to be truncated by
       the amount of data actually in the read buffer. */
    avail = TSIOBufferReaderAvail(TSVIOReaderGet(write_vio));
    if (avail == TS_ERROR) {
      TSError("Unable to get the number of bytes availabe for reading");
    } else {
      if (towrite > avail) {
        towrite = avail;
      }

      if (towrite > 0) {
        /* Copy the data from the read buffer to the input buffer. */
        if (TSIOBufferCopy(data->input_buf, TSVIOReaderGet(write_vio), towrite, 0) == TS_ERROR) {
          TSError("Error in Copying the buffer");
        } else {

          /* Tell the read buffer that we have read the data and are no
             longer interested in it. */
          if (TSIOBufferReaderConsume(TSVIOReaderGet(write_vio), towrite) != TS_SUCCESS) {
            TSError("Unable to consume bytes from the buffer");
          }

          /* Modify the write VIO to reflect how much data we've
             completed. */
          if (TSVIONDoneSet(write_vio, TSVIONDoneGet(write_vio) + towrite) != TS_SUCCESS) {
            TSError("Unable to modify the write VIO to reflect how much data we have completed");
          }
        }
      }
    }
  } else {
    if (towrite == TS_ERROR) {
      TSError("TSVIONTodoGet returns TS_ERROR");
      return 0;
    }
  }

  /* Now we check the write VIO to see if there is data left to
     read. */
  if (TSVIONTodoGet(write_vio) > 0) {
    /* Call back the write VIO continuation to let it know that we
       are ready for more data. */
    TSContCall(TSVIOContGet(write_vio), TS_EVENT_VCONN_WRITE_READY, write_vio);
  } else {
    /* Call back the write VIO continuation to let it know that we
       have completed the write operation. */
    TSContCall(TSVIOContGet(write_vio), TS_EVENT_VCONN_WRITE_COMPLETE, write_vio);

    /* start compression... */
    return transform_connect(contp, data);
  }

  return 0;
}

static int
transform_connect_event(TSCont contp, TransformData * data, TSEvent event, void *edata)
{
  switch (event) {
  case TS_EVENT_NET_CONNECT:
    data->pending_action = NULL;
    data->server_vc = (TSVConn) edata;
    return transform_write(contp, data);
  case TS_EVENT_NET_CONNECT_FAILED:
    data->pending_action = NULL;
    return transform_bypass(contp, data);
  default:
    break;
  }

  return 0;
}

static int
transform_write_event(TSCont contp, TransformData * data, TSEvent event, void *edata)
{
  switch (event) {
  case TS_EVENT_VCONN_WRITE_READY:
    if (TSVIOReenable(data->server_vio) != TS_SUCCESS) {
      TSError("Unable to reenable the server vio in TS_EVENT_VCONN_WRITE_READY");
    }
    break;
  case TS_EVENT_VCONN_WRITE_COMPLETE:
    return transform_read_status(contp, data);
  default:
    /* An error occurred while writing to the server. Close down
       the connection to the server and bypass. */
    return transform_bypass(contp, data);
  }

  return 0;
}

static int
transform_read_status_event(TSCont contp, TransformData * data, TSEvent event, void *edata)
{
  switch (event) {
  case TS_EVENT_ERROR:
  case TS_EVENT_VCONN_EOS:
    return transform_bypass(contp, data);
  case TS_EVENT_VCONN_READ_COMPLETE:
    if (TSIOBufferReaderAvail(data->output_reader) == sizeof(int)) {
      TSIOBufferBlock blk;
      char *buf;
      void *buf_ptr;
      int64_t avail;
      int64_t read_nbytes = sizeof(int);
      int64_t read_ndone = 0;

      buf_ptr = &data->content_length;
      while (read_nbytes > 0) {
        blk = TSIOBufferReaderStart(data->output_reader);
        if (blk == TS_ERROR_PTR) {
          TSError("Error in Getting the pointer to starting of reader block");
        } else {
          buf = (char *) TSIOBufferBlockReadStart(blk, data->output_reader, &avail);
          if (buf != TS_ERROR_PTR) {
            read_ndone = (avail >= read_nbytes) ? read_nbytes : avail;
            memcpy(buf_ptr, buf, read_ndone);
            if (read_ndone > 0) {
              if (TSIOBufferReaderConsume(data->output_reader, read_ndone) != TS_SUCCESS) {
                TSError("Error in consuming data from the buffer");
              } else {
                read_nbytes -= read_ndone;
                /* move ptr frwd by read_ndone bytes */
                buf_ptr = (char *) buf_ptr + read_ndone;
              }
            }
          } else {
            TSError("TSIOBufferBlockReadStart returns TS_ERROR_PTR");
          }
        }
      }
      data->content_length = ntohl(data->content_length);
      return transform_read(contp, data);
    }
    return transform_bypass(contp, data);
  default:
    break;
  }

  return 0;
}

static int
transform_read_event(TSCont contp, TransformData * data, TSEvent event, void *edata)
{
  switch (event) {
  case TS_EVENT_ERROR:
    if (TSVConnAbort(data->server_vc, 1) != TS_SUCCESS) {
      TSError("TSVConnAbort doesn't return TS_SUCCESS on server VConnection during TS_EVENT_ERROR");
    }
    data->server_vc = NULL;
    data->server_vio = NULL;

    if (TSVConnAbort(data->output_vc, 1) != TS_SUCCESS) {
      TSError("TSVConnAbort doesn't return TS_SUCCESS on output VConnection during TS_EVENT_ERROR");
    }
    data->output_vc = NULL;
    data->output_vio = NULL;
    break;
  case TS_EVENT_VCONN_EOS:
    if (TSVConnAbort(data->server_vc, 1) != TS_SUCCESS) {
      TSError("TSVConnAbort doesn't return TS_SUCCESS on server VConnection during TS_EVENT_VCONN_EOS");
    }
    data->server_vc = NULL;
    data->server_vio = NULL;

    if (TSVConnAbort(data->output_vc, 1) != TS_SUCCESS) {
      TSError("TSVConnAbort doesn't return TS_SUCCESS on output VConnection during TS_EVENT_VCONN_EOS");
    }
    data->output_vc = NULL;
    data->output_vio = NULL;
    break;
  case TS_EVENT_VCONN_READ_COMPLETE:
    if (TSVConnClose(data->server_vc) != TS_SUCCESS) {
      TSError("TSVConnClose doesn't return TS_SUCCESS on TS_EVENT_VCONN_READ_COMPLETE");
    }
    data->server_vc = NULL;
    data->server_vio = NULL;

    if (TSVIOReenable(data->output_vio) != TS_SUCCESS) {
      TSError("TSVIOReneable doesn't return TS_SUCCESS on TS_EVENT_VCONN_READ_COMPLETE");
    }
    break;
  case TS_EVENT_VCONN_READ_READY:
    if (TSVIOReenable(data->output_vio) != TS_SUCCESS) {
      TSError("TSVIOReneable doesn't return TS_SUCCESS on TS_EVENT_VCONN_READ_READY");
    }
    break;
  case TS_EVENT_VCONN_WRITE_COMPLETE:
    if (TSVConnShutdown(data->output_vc, 0, 1) != TS_SUCCESS) {
      TSError("TSVConnShutdown doesn't return TS_SUCCESS during TS_EVENT_VCONN_WRITE_COMPLETE");
    }
    break;
  case TS_EVENT_VCONN_WRITE_READY:
    if (TSVIOReenable(data->server_vio) != TS_SUCCESS) {
      TSError("TSVIOReneable doesn't return TS_SUCCESS while reenabling on TS_EVENT_VCONN_WRITE_READY");
    }
    break;
  default:
    break;
  }

  return 0;
}

static int
transform_bypass_event(TSCont contp, TransformData * data, TSEvent event, void *edata)
{
  switch (event) {
  case TS_EVENT_VCONN_WRITE_COMPLETE:
    if (TSVConnShutdown(data->output_vc, 0, 1) != TS_SUCCESS) {
      TSError("Error in shutting down the VConnection while bypassing the event");
    }
    break;
  case TS_EVENT_VCONN_WRITE_READY:
  default:
    if (TSVIOReenable(data->output_vio) != TS_SUCCESS) {
      TSError("Error in re-enabling the VIO while bypassing the event");
    }
    break;
  }

  return 0;
}

static int
transform_handler(TSCont contp, TSEvent event, void *edata)
{
  /* Check to see if the transformation has been closed by a call to
     TSVConnClose. */
  if (TSVConnClosedGet(contp)) {
    transform_destroy(contp);
    return 0;
  } else {
    TransformData *data;
    int val = 0;

    data = (TransformData *) TSContDataGet(contp);
    if ((data == NULL) && (data == TS_ERROR_PTR)) {
      TSError("Didn't get Continuation's Data. Ignoring Event..");
      return 0;
    }
    do {
      switch (data->state) {
      case STATE_BUFFER:
        val = transform_buffer_event(contp, data, event, edata);
        break;
      case STATE_CONNECT:
        val = transform_connect_event(contp, data, event, edata);
        break;
      case STATE_WRITE:
        val = transform_write_event(contp, data, event, edata);
        break;
      case STATE_READ_STATUS:
        val = transform_read_status_event(contp, data, event, edata);
        break;
      case STATE_READ:
        val = transform_read_event(contp, data, event, edata);
        break;
      case STATE_BYPASS:
        val = transform_bypass_event(contp, data, event, edata);
        break;
      }
    } while (val);
  }

  return 0;
}

static int
request_ok(TSHttpTxn txnp)
{
  /* Is the initial client request OK for transformation. This is a
     good place to check accept headers to see if the client can
     accept a transformed document. */
  return 1;
}

static int
cache_response_ok(TSHttpTxn txnp)
{
  /* Is the response we're reading from cache OK for
   * transformation. This is a good place to check the cached
   * response to see if it is transformable. The default
   * behavior is to cache transformed content; therefore
   * to avoid transforming twice we will not transform
   * content served from the cache.
   */
  return 0;
}

static int
server_response_ok(TSHttpTxn txnp)
{
  /* Is the response the server sent OK for transformation. This is
   * a good place to check the server's response to see if it is
   * transformable. In this example, we will transform only "200 OK"
   * responses.
   */

  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSHttpStatus resp_status;

  if (TSHttpTxnServerRespGet(txnp, &bufp, &hdr_loc) == 0) {
    TSError("Unable to get handle to Server Response");
    return 0;
  }

  if ((resp_status = TSHttpHdrStatusGet(bufp, hdr_loc)) == (TSHttpStatus)TS_ERROR) {
    TSError("Error in Getting Status from Server response");
    if (TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc) != TS_SUCCESS) {
      TSError("Unable to release handle to server request");
    }
    return 0;
  }

  if (TS_HTTP_STATUS_OK == resp_status) {
    if (TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc) != TS_SUCCESS) {
      TSError("Unable to release handle to server request");
    }
    return 1;
  } else {
    if (TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc) != TS_SUCCESS) {
      TSError("Unable to release handle to server request");
    }
    return 0;
  }
}

static int
transform_plugin(TSCont contp, TSEvent event, void *edata)
{
  TSHttpTxn txnp = (TSHttpTxn) edata;

  switch (event) {
  case TS_EVENT_HTTP_READ_REQUEST_HDR:
    if (request_ok(txnp)) {
      if (TSHttpTxnHookAdd(txnp, TS_HTTP_READ_CACHE_HDR_HOOK, contp) != TS_SUCCESS) {
        TSError("Unable to add continuation to hook " "TS_HTTP_READ_CACHE_HDR_HOOK for this transaction");
      }
      if (TSHttpTxnHookAdd(txnp, TS_HTTP_READ_RESPONSE_HDR_HOOK, contp) != TS_SUCCESS) {
        TSError("Unable to add continuation to hook " "TS_HTTP_READ_RESPONSE_HDR_HOOK for this transaction");
      }
    }
    if (TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE) != TS_SUCCESS) {
      TSError("Error in re-enabling transaction at TS_HTTP_READ_REQUEST_HDR_HOOK");
    }
    break;
  case TS_EVENT_HTTP_READ_CACHE_HDR:
    if (cache_response_ok(txnp)) {
      if (TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, transform_create(txnp)) != TS_SUCCESS) {
        TSError("Unable to add continuation to tranformation hook " "for this transaction");
      }
    }
    if (TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE) != TS_SUCCESS) {
      TSError("Error in re-enabling transaction at TS_HTTP_READ_CACHE_HDR_HOOK");
    }
    break;
  case TS_EVENT_HTTP_READ_RESPONSE_HDR:
    if (server_response_ok(txnp)) {
      if (TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, transform_create(txnp)) != TS_SUCCESS) {
        TSError("Unable to add continuation to tranformation hook " "for this transaction");
      }
    }
    if (TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE) != TS_SUCCESS) {
      TSError("Error in re-enabling transaction at TS_HTTP_READ_RESPONSE_HDR_HOOK");
    }
    break;
  default:
    break;
  }
  return 0;
}

int
check_ts_version()
{

  const char *ts_version = TSTrafficServerVersionGet();
  int result = 0;

  if (ts_version) {
    int major_ts_version = 0;
    int minor_ts_version = 0;
    int patch_ts_version = 0;

    if (sscanf(ts_version, "%d.%d.%d", &major_ts_version, &minor_ts_version, &patch_ts_version) != 3) {
      return 0;
    }

    /* Since this is an TS-SDK 2.0 plugin, we need at
       least Traffic Server 2.0 to run */
    if (major_ts_version >= 2) {
      result = 1;
    }
  }

  return result;
}

void
TSPluginInit(int argc, const char *argv[])
{
  TSPluginRegistrationInfo info;
  TSCont cont;

  info.plugin_name = "server-transform";
  info.vendor_name = "MyCompany";
  info.support_email = "ts-api-support@MyCompany.com";

  if (!TSPluginRegister(TS_SDK_VERSION_3_0, &info)) {
    TSError("Plugin registration failed.\n");
  }

  if (!check_ts_version()) {
    TSError("Plugin requires Traffic Server 3.0 or later\n");
    return;
  }

  /* connect to the echo port on localhost */
  server_ip = (127 << 24) | (0 << 16) | (0 << 8) | (1);
  server_ip = htonl(server_ip);
  server_port = 7;

  if ((cont = TSContCreate(transform_plugin, NULL)) == TS_ERROR_PTR) {
    TSError("Unable to create continuation. Aborting...");
    return;
  }

  if (TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont) == TS_ERROR) {
    TSError("Unable to add the continuation to the hook. Aborting...");
    if (TSContDestroy(cont) == TS_ERROR) {
      TSError("Error in Destroying the continuation.");
    }
  }
}
