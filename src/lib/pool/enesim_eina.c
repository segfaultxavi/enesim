/* ENESIM - Direct Rendering Library
 * Copyright (C) 2007-2008 Jorge Luis Zapata
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

#include "Enesim.h"
#include "enesim_private.h"
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
typedef struct _Enesim_Eina_Pool
{
	Eina_Mempool *mp;
} Enesim_Eina_Pool;

static Eina_Bool _data_alloc(void *prv, Enesim_Buffer_Backend *buffer_backend,
		Enesim_Buffer_Format fmt, uint32_t w, uint32_t h)
{
	Enesim_Buffer_Data *data;
	Enesim_Eina_Pool *thiz = prv;
	void *alloc_data;
	size_t bytes;

	buffer_backend->backend = ENESIM_BACKEND_SOFTWARE;
	data = &buffer_backend->data.sw_data;
	bytes = enesim_buffer_format_size_get(fmt, w, h);
	alloc_data = eina_mempool_malloc(thiz->mp, bytes);
	switch (fmt)
	{
		case ENESIM_CONVERTER_ARGB8888:
		data->argb8888.plane0 = alloc_data;
		data->argb8888.plane0_stride = w;
		break;

		case ENESIM_CONVERTER_ARGB8888_PRE:
		data->argb8888_pre.plane0 = alloc_data;
		data->argb8888_pre.plane0_stride = w;
		break;

		case ENESIM_CONVERTER_RGB565:
		data->rgb565.plane0 = alloc_data;
		data->rgb565.plane0_stride = w;
		break;

		case ENESIM_CONVERTER_RGB888:
		data->rgb565.plane0 = alloc_data;
		data->rgb565.plane0_stride = w;
		break;

		case ENESIM_CONVERTER_A8:
		case ENESIM_CONVERTER_GRAY:
		printf("TODO\n");
		break;
	}
	return EINA_TRUE;
}

static void _data_free(void *prv, Enesim_Buffer_Backend *buffer_backend,
		Enesim_Buffer_Format fmt)
{
	Enesim_Eina_Pool *thiz = prv;
	Enesim_Buffer_Data *data;
	data = &buffer_backend->data.sw_data;

	switch (fmt)
	{
		case ENESIM_CONVERTER_ARGB8888:
		eina_mempool_free(thiz->mp, data->argb8888_pre.plane0);
		break;

		case ENESIM_CONVERTER_ARGB8888_PRE:
		eina_mempool_free(thiz->mp, data->argb8888_pre.plane0);
		break;

		case ENESIM_CONVERTER_RGB565:
		case ENESIM_CONVERTER_RGB888:
		case ENESIM_CONVERTER_A8:
		case ENESIM_CONVERTER_GRAY:
		printf("TODO\n");
		break;
	}
}

static void _free(void *prv)
{
	Enesim_Eina_Pool *thiz = prv;

	eina_mempool_del(thiz->mp);
	free(thiz);
}

static Enesim_Pool_Descriptor _descriptor = {
	/* .data_alloc = */ _data_alloc,
	/* .data_free =  */ _data_free,
	/* .free =       */ NULL
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
EAPI Enesim_Pool * enesim_pool_eina_new(Eina_Mempool *mp)
{
	Enesim_Eina_Pool *thiz;
	Enesim_Pool *p;

	thiz = calloc(1, sizeof(Enesim_Eina_Pool));
	thiz->mp = mp;

	p = enesim_pool_new(&_descriptor, thiz);
	if (!p)
	{
		free(thiz);
		return NULL;
	}

	return p;
}
