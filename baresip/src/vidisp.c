/**
 * @file vidisp.c  Video Display
 *
 * Copyright (C) 2010 Alfred E. Heggestad
 */
#include <re.h>
#include <baresip.h>
#include "core.h"


/** Video Display state */
struct vidisp_st {
	struct vidisp *vd;  /**< Video Display */
};


static struct list vidispl = LIST_INIT;


static void destructor(void *arg)
{
	struct vidisp *vd = arg;

	list_unlink(&vd->le);
}


/**
 * Register a Video output display
 *
 * @param vp       Pointer to allocated Video Display
 * @param name     Name of Video Display
 * @param alloch   Allocation handler
 * @param updateh  Update handler
 * @param disph    Display handler
 * @param hideh    Hide-window handler
 *
 * @return 0 if success, otherwise errorcode
 */
int vidisp_register(struct vidisp **vp, const char *name,
		    vidisp_alloc_h *alloch, vidisp_update_h *updateh,
		    vidisp_disp_h *disph, vidisp_hide_h *hideh)
{
	struct vidisp *vd;

	if (!vp)
		return EINVAL;

	vd = mem_zalloc(sizeof(*vd), destructor);
	if (!vd)
		return ENOMEM;

	list_append(&vidispl, &vd->le, vd);

	vd->name    = name;
	vd->alloch  = alloch;
	vd->updateh = updateh;
	vd->disph   = disph;
	vd->hideh   = hideh;

	(void)re_printf("vidisp: %s\n", name);

	*vp = vd;
	return 0;
}


const struct vidisp *vidisp_find(const char *name)
{
	struct le *le;

	for (le = vidispl.head; le; le = le->next) {
		struct vidisp *vd = le->data;

		if (name && 0 != str_casecmp(name, vd->name))
			continue;

		/* Found */
		return vd;
	}

	return NULL;
}


/**
 * Allocate a video display state
 *
 * @param stp     Pointer to allocated display state
 * @param name    Name of video display
 * @param prm     Video display parameters (optional)
 * @param dev     Display device
 * @param resizeh Window resize handler
 * @param arg     Handler argument
 *
 * @return 0 if success, otherwise errorcode
 */
int vidisp_alloc(struct vidisp_st **stp, const char *name,
		 struct vidisp_prm *prm, const char *dev,
		 vidisp_resize_h *resizeh, void *arg)
{
	struct vidisp *vd = (struct vidisp *)vidisp_find(name);
	if (!vd)
		return ENOENT;

	return vd->alloch(stp, vd, prm, dev, resizeh, arg);
}


/**
 * Display a video frame
 *
 * @param st    Video display state
 * @param title Display title
 * @param frame Video frame
 *
 * @return 0 if success, otherwise errorcode
 */
int vidisp_display(struct vidisp_st *st, const char *title,
		   const struct vidframe *frame, uint64_t timestamp)
{
	struct vidisp *vd;

	if (!st || !frame)
		return EINVAL;

	vd = st->vd;

	//re_printf("%s/%04d frame %p\n", __FILE__, __LINE__, frame);

	if (vd == NULL)
		return EINVAL;

	return vd->disph(st, title, frame, timestamp);
}


struct vidisp *vidisp_get(struct vidisp_st *st)
{
	return st ? st->vd : NULL;
}
