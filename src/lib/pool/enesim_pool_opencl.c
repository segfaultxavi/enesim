/* ENESIM - Drawing Library
 * Copyright (C) 2007-2013 Jorge Luis Zapata
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.
 * If not, see <http://www.gnu.org/licenses/>.
 */
#include "enesim_private.h"

#include "enesim_main.h"
#include "enesim_pool.h"
#include "enesim_buffer.h"

#include "Enesim_OpenCL.h"

#include "enesim_pool_private.h"
#include "enesim_buffer_private.h"
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
/** @cond internal */
typedef struct _Enesim_OpenCL_Pool
{
	cl_context context;
	cl_device_id device;
	cl_command_queue queue;
} Enesim_OpenCL_Pool;

static Eina_Bool _data_alloc(void *prv, Enesim_Backend *backend,
		void **backend_data,
		Enesim_Buffer_Format fmt, uint32_t w, uint32_t h)
{
	Enesim_OpenCL_Pool *thiz = prv;
	Enesim_Buffer_OpenCL_Data *data;
	cl_mem i;
	cl_int ret;
	cl_image_format format;

	format.image_channel_order = CL_ARGB;
	format.image_channel_data_type = CL_UNSIGNED_INT8;
	i = clCreateImage2D(thiz->context, CL_MEM_READ_WRITE,
			&format, w, h, 0, NULL, &ret);
	if (ret != CL_SUCCESS)
	{
		DBG("impossible to create the image %d", ret);
		return EINA_FALSE;
	}
	*backend = ENESIM_BACKEND_OPENCL;
	data = malloc(sizeof(Enesim_Buffer_OpenCL_Data));
	*backend_data = data;
	data->mem = i;
	data->context = thiz->context;
	data->device = thiz->device;
	data->queue = thiz->queue;

	DBG("Image created correctly");
	return EINA_TRUE;
}

static void _data_free(void *prv, void *backend_data,
		Enesim_Buffer_Format fmt,
		Eina_Bool external_allocated)
{
	Enesim_Buffer_OpenCL_Data *data = backend_data;

	clReleaseMemObject(data->mem);
}

static Eina_Bool _data_get(void *prv, void *backend_data,
		Enesim_Buffer_Format fmt,
		uint32_t w, uint32_t h,
		Enesim_Buffer_Sw_Data *dst)
{
	Enesim_Buffer_OpenCL_Data *data = backend_data;
	size_t origin[3];
	size_t region[3];
	size_t size;
	cl_int ret;

	origin[0] = 0;
	origin[1] = 0;
	origin[2] = 0;

	region[0] = w;
	region[1] = h;
	region[2] = 1;

	clGetImageInfo(data->mem, CL_IMAGE_ROW_PITCH, sizeof(size_t), &size, NULL);
	DBG("row pitch %d", size);
	dst->argb8888_pre.plane0 = calloc(size * h, sizeof(uint8_t));
	ret = clEnqueueReadImage(data->queue, data->mem, CL_TRUE, origin, region, 0, 0, dst->argb8888_pre.plane0, 0, NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		DBG("Failed getting the surface");
		return EINA_FALSE;
	}
	dst->argb8888_pre.plane0_stride = size;

	return EINA_TRUE;
}

static void _free(void *prv)
{
	Enesim_OpenCL_Pool *thiz = prv;

	free(thiz);
}

static Enesim_Pool_Descriptor _descriptor = {
	/* .data_alloc = */ _data_alloc,
	/* .data_free =  */ _data_free,
	/* .data_from =  */ NULL,
	/* .data_get =   */ _data_get,
	/* .data_put =   */ NULL,
	/* .free =       */ _free,
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/** @endcond */
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
EAPI Enesim_Pool * enesim_pool_opencl_new(void)
{
	Enesim_OpenCL_Pool *thiz;
	Enesim_Pool *p;
	cl_context context;
	cl_command_queue queue;
	cl_device_id device_id;
	cl_int ret;
	cl_platform_id platform_id;
	cl_uint ret_num_devices;
	cl_uint ret_num_platforms;

	clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
	ret = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_DEFAULT, 1,
            &device_id, &ret_num_devices);
	if (ret != CL_SUCCESS)
	{
		DBG("impossible to get the devices");
 		goto end;
	}
	context = clCreateContext(0, 1, &device_id, NULL, NULL, &ret);
	if (ret != CL_SUCCESS)
	{
		DBG("impossible to get the context");
		goto end;
	}
	queue = clCreateCommandQueue(context, device_id, 0, &ret);
	if (ret != CL_SUCCESS)
	{
		DBG("impossible to get the command queue");
		goto end;
	}

	DBG("Everything went ok");
	thiz = calloc(1, sizeof(Enesim_OpenCL_Pool));
	thiz->context = context;
	thiz->device = device_id;
	thiz->queue = queue;

	p = enesim_pool_new(&_descriptor, thiz);
	if (!p)
	{
		free(thiz);
		return NULL;
	}

	return p;
end:
	return NULL;
}

