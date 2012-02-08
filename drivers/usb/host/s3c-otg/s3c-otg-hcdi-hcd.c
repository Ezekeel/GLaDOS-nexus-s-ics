/****************************************************************************
 *  (C) Copyright 2008 Samsung Electronics Co., Ltd., All rights reserved
 *
 * @file   s3c-otg-hcdi-hcd.c
 * @brief  implementation of structure hc_drive \n
 * @version
 *  -# Jun 11,2008 v1.0 by SeungSoo Yang (ss1.yang@samsung.com) \n
 *	  : Creating the initial version of this code \n
 *  -# Jul 15,2008 v1.2 by SeungSoo Yang (ss1.yang@samsung.com) \n
 *	  : Optimizing for performance \n
 *  -# Aug 18,2008 v1.3 by SeungSoo Yang (ss1.yang@samsung.com) \n
 *	  : Modifying for successful rmmod & disconnecting \n
 * @see None
 ****************************************************************************/
/****************************************************************************
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/

#include "s3c-otg-hcdi-hcd.h"

/**
 * otg_hcd_init_modules(struct sec_otghost *otghost)
 *
 * @brief call other modules' init functions
 *
 * @return PASS : If success \n
 *         FAIL : If fail \n
 */
int otg_hcd_init_modules(struct sec_otghost *otghost)
{
	unsigned long	spin_lock_flag = 0;

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "otg_hcd_init_modules\n");

	spin_lock_init(&otghost->lock);

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);

	init_transfer();
	init_scheduler();
	oci_init(otghost);

	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);

	return USB_ERR_SUCCESS;
};

/**
 * void otg_hcd_deinit_modules(struct sec_otghost *otghost)
 *
 * @brief call other modules' de-init functions
 *
 * @return PASS : If success \n
 *         FAIL : If fail \n
 */
void otg_hcd_deinit_modules(struct sec_otghost *otghost)
{
	unsigned long	spin_lock_flag = 0;

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "otg_hcd_deinit_modules \n");

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);

	deinit_transfer(otghost);

	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
}

/**
 * irqreturn_t	(*s5pc110_otghcd_irq) (struct usb_hcd *hcd)
 *
 * @brief interrupt handler of otg irq
 *
 * @param  [in] hcd : pointer of usb_hcd
 *
 * @return IRQ_HANDLED  \n
 */
irqreturn_t	s5pc110_otghcd_irq(struct usb_hcd *hcd)
{
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_irq \n");

	spin_lock_otg(&otghost->lock);
	otg_handle_interrupt(hcd);
	spin_unlock_otg(&otghost->lock);

	return	IRQ_HANDLED;
}

/**
 * int s5pc110_otghcd_start(struct usb_hcd *hcd)
 *
 * @brief  initialize and start otg hcd
 *
 * @param  [in] usb_hcd_p : pointer of usb_hcd
 *
 * @return USB_ERR_SUCCESS : If success \n
 *         USB_ERR_FAIL : If call fail \n
 */
int	s5pc110_otghcd_start(struct usb_hcd *usb_hcd_p)
{
	struct	usb_bus *usb_bus_p;
	struct sec_otghost *otghost = hcd_to_sec_otghost(usb_hcd_p);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_start \n");

	usb_bus_p = hcd_to_bus(usb_hcd_p);

	/* Initialize and connect root hub if one is not already attached */
	if (usb_bus_p->root_hub) {
		otg_dbg(OTG_DBG_OTGHCDI_HCD, "OTG HCD Has Root Hub\n");

		/* Inform the HUB driver to resume. */
		otg_usbcore_resume_roothub();
	} else {
		otg_err(OTG_DBG_OTGHCDI_HCD,
			       	"OTG HCD Does Not Have Root Hub\n");
		return USB_ERR_FAIL;
	}

	set_bit(HCD_FLAG_POLL_RH,&usb_hcd_p->flags);
	usb_hcd_p->uses_new_polling = 1;

	/* init bus state	before enable irq */
	usb_hcd_p->state = HC_STATE_RUNNING;

	oci_start(otghost); /* enable irq */

	return USB_ERR_SUCCESS;
}

/**
 * void	s5pc110_otghcd_stop(struct usb_hcd *hcd)
 *
 * @brief  deinitialize and stop otg hcd
 *
 * @param  [in] hcd : pointer of usb_hcd
 *
 */
void s5pc110_otghcd_stop(struct usb_hcd *hcd)
{
	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_stop \n");

	otg_hcd_deinit_modules(otghost);

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);

	oci_stop(otghost);
        root_hub_feature(hcd, 0, ClearPortFeature, USB_PORT_FEAT_POWER, NULL);

	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
}

/**
 * void	s5pc110_otghcd_shutdown(struct usb_hcd *hcd)
 *
 * @brief  shutdown otg hcd
 *
 * @param  [in] usb_hcd_p : pointer of usb_hcd
 *
 */
void s5pc110_otghcd_shutdown(struct usb_hcd *usb_hcd_p)
{
	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(usb_hcd_p);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_shutdown \n");
	otg_hcd_deinit_modules(otghost);

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);

	oci_stop(otghost);
        root_hub_feature(usb_hcd_p, 0, ClearPortFeature, USB_PORT_FEAT_POWER, NULL);

	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);

	free_irq(IRQ_OTG, usb_hcd_p);
	usb_hcd_p->state = HC_STATE_HALT;
	otg_usbcore_hc_died();
}


/**
 * int s5pc110_otghcd_get_frame_number(struct usb_hcd *hcd)
 *
 * @brief  get currnet frame number
 *
 * @param  [in] hcd : pointer of usb_hcd
 *
 * @return ret : frame number \n
 */
int s5pc110_otghcd_get_frame_number(struct usb_hcd *hcd)
{
	int 	ret = 0;
	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_get_frame_number \n");

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);
	ret = oci_get_frame_num();
	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);

	return ret;
}


/**
 * int s5pc110_otghcd_urb_enqueue()
 *
 * @brief  enqueue a urb to otg hcd
 *
 * @param  [in] hcd : pointer of usb_hcd
 *		   [in] ep : pointer of usb_host_endpoint
 *		   [in] urb : pointer of urb
 *		   [in] mem_flags : type of gfp_t
 *
 * @return USB_ERR_SUCCESS : If success \n
 *         USB_ERR_FAIL : If call fail \n
 */
int s5pc110_otghcd_urb_enqueue (struct usb_hcd *hcd,
	      	 		struct urb *urb,
			       	gfp_t mem_flags)
{
	int	ret_val = 0;
	u32	trans_flag = 0;
	u32 	return_td_addr = 0;
	u8	dev_speed, ed_type = 0, additional_multi_count;
	u16	max_packet_size;

	u8 	dev_addr = 0;
	u8 	ep_num = 0;
	bool 	f_is_ep_in = true;
	u8 	interval = 0;
	u32 	sched_frame = 0;
	u8 	hub_addr = 0;
	u8 	hub_port = 0;
	bool	f_is_do_split = false;
	ed_t	*target_ed = NULL;
	isoch_packet_desc_t *new_isoch_packet_desc = NULL;
	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	if (!otghost->port_flag.b.port_connect_status) {
		printk(KERN_ERR"%s %d\n", __func__, __LINE__);
		return USB_ERR_NOIO;
	}

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_urb_enqueue \n");

	/* check ep has ed_t or not */
	if(!(urb->ep->hcpriv)) {
		/* for getting dev_speed */
		switch (urb->dev->speed) {
		case USB_SPEED_HIGH :
			otg_dbg(OTG_DBG_OTGHCDI_HCD, "HS_OTG \n");
			dev_speed = HIGH_SPEED_OTG;
			break;

		case USB_SPEED_FULL :
			otg_dbg(OTG_DBG_OTGHCDI_HCD, "FS_OTG \n");
			dev_speed = FULL_SPEED_OTG;
			break;

		case USB_SPEED_LOW :
			otg_dbg(OTG_DBG_OTGHCDI_HCD, "LS_OTG \n");
			dev_speed = LOW_SPEED_OTG;
			break;

		default:
			otg_err(OTG_DBG_OTGHCDI_HCD,
				"unKnown Device Speed \n");
                        spin_unlock_irq_save_otg(&otghost->lock, 
						spin_lock_flag);
			return USB_ERR_FAIL;
		}

		/* for getting ed_type */
		switch (usb_pipetype(urb->pipe)) {
		case PIPE_BULK :
			otg_dbg(OTG_DBG_OTGHCDI_HCD, "bulk transfer \n");
			ed_type = BULK_TRANSFER;
			break;

		case PIPE_INTERRUPT :
			otg_dbg(OTG_DBG_OTGHCDI_HCD, "interrupt transfer \n");
			ed_type = INT_TRANSFER;
			break;

		case PIPE_CONTROL :
			otg_dbg(OTG_DBG_OTGHCDI_HCD, "control transfer \n");
			ed_type = CONTROL_TRANSFER;
			break;

		case PIPE_ISOCHRONOUS :
			otg_dbg(OTG_DBG_OTGHCDI_HCD, "isochronous transfer \n");
			ed_type = ISOCH_TRANSFER;
			break;
		default:
			otg_err(OTG_DBG_OTGHCDI_HCD, "unKnown ep type \n");
                        spin_unlock_irq_save_otg(&otghost->lock, 
							spin_lock_flag);
			return USB_ERR_FAIL;
		}

		max_packet_size = usb_maxpacket(urb->dev, urb->pipe,
		       				!(usb_pipein(urb->pipe)));
		additional_multi_count = ((max_packet_size) >> 11) & 0x03;
		dev_addr = usb_pipedevice(urb->pipe);
		ep_num = usb_pipeendpoint(urb->pipe);
		f_is_ep_in = usb_pipein(urb->pipe) ? true : false;
		interval = (u8)(urb->interval);
		sched_frame = (u8)(urb->start_frame);

		/* check */
		if(urb->dev->tt == NULL) {
			otg_dbg(OTG_DBG_OTGHCDI_HCD, "urb->dev->tt == NULL\n");
			hub_port = 0; /* u8 hub_port */
			hub_addr = 0; /* u8 hub_addr */
		}
		else {
			hub_port = (u8)(urb->dev->ttport);
			if (urb->dev->tt->hub) {
				if (((dev_speed == FULL_SPEED_OTG) ||
				    (dev_speed == LOW_SPEED_OTG)) &&
				    	(urb->dev->tt) && (urb->dev->tt->hub->devnum != 1)) {
					if (otghost->is_hs) // only allow split transactions in HS mode
						f_is_do_split = true;
				}
				hub_addr = (u8)(urb->dev->tt->hub->devnum);
			}
			if (urb->dev->tt->multi) {
				hub_addr = 0x80;
			}
		}
		otg_dbg(OTG_DBG_OTGHCDI_HCD,
			"dev_spped =%d, hub_port=%d, hub_addr=%d\n",
				dev_speed, hub_port, hub_addr);

		ret_val = create_ed(&target_ed);
		if(ret_val != USB_ERR_SUCCESS) {
			otg_err(OTG_DBG_OTGHCDI_HCD,
				"fail to create_ed() \n");
                        spin_unlock_irq_save_otg(&otghost->lock, 
							spin_lock_flag);
			return ret_val;
		}

		ret_val = init_ed( target_ed,
				dev_addr,
				ep_num,
				f_is_ep_in,
				dev_speed,
				ed_type,
				max_packet_size,
				additional_multi_count,
				interval,
				sched_frame,
				hub_addr,
				hub_port,
				f_is_do_split,
				(void *)urb->ep);

		if(ret_val != USB_ERR_SUCCESS) {
			otg_err(OTG_DBG_OTGHCDI_HCD,
				"fail to init_ed() :err = %d \n",(int)ret_val);
			otg_mem_free(target_ed);
                        spin_unlock_irq_save_otg(&otghost->lock, 
			      			 	spin_lock_flag);
			return USB_ERR_FAIL;
		}

		urb->ep->hcpriv = (void *)(target_ed);
	} /* if(!(ep->hcpriv)) */
	else {
		dev_addr = usb_pipedevice(urb->pipe);
		if(((ed_t *)(urb->ep->hcpriv))->ed_desc.device_addr != dev_addr) {
			((ed_t *)urb->ep->hcpriv)->ed_desc.device_addr = dev_addr;
		}
	}

	target_ed = (ed_t *)urb->ep->hcpriv;

	if(urb->transfer_flags & URB_SHORT_NOT_OK)
		trans_flag += USB_TRANS_FLAG_NOT_SHORT;
	if  (urb->transfer_flags & URB_ISO_ASAP)
		trans_flag += USB_TRANS_FLAG_ISO_ASYNCH;

	if(ed_type == ISOCH_TRANSFER) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "ISO not yet supported \n");
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		return USB_ERR_FAIL;
	}

	if (!HC_IS_RUNNING(hcd->state)) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "!HC_IS_RUNNING(hcd->state) \n");
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		return -USB_ERR_NODEV;
	}

	/* in case of unlink-during-submit */
	if (urb->status != -EINPROGRESS) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "urb->status is -EINPROGRESS\n");
		urb->hcpriv = NULL;
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		usb_hcd_giveback_urb(hcd, urb, urb->status);
		return USB_ERR_SUCCESS;
	}

	ret_val = issue_transfer(otghost, target_ed, (void *)NULL, (void *)NULL,
		    trans_flag,
		    (usb_pipetype(urb->pipe) == PIPE_CONTROL)?true:false,
		    (u32)urb->setup_packet, (u32)urb->setup_dma,
		    (u32)urb->transfer_buffer, (u32)urb->transfer_dma,
		    (u32)urb->transfer_buffer_length,
		    (u32)urb->start_frame,(u32)urb->number_of_packets,
		    new_isoch_packet_desc, (void *)urb, &return_td_addr);

	if(ret_val != USB_ERR_SUCCESS) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "fail to issue_transfer() \n");
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		return USB_ERR_FAIL;
	}
	urb->hcpriv = (void *)return_td_addr;

	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
	return USB_ERR_SUCCESS;
}

/**
 * int s5pc110_otghcd_urb_dequeue(struct usb_hcd *_hcd, struct urb *_urb )
 *
 * @brief  dequeue a urb to otg
 *
 * @param  [in] _hcd : pointer of usb_hcd
 *		   [in] _urb : pointer of urb
 *
 * @return USB_ERR_SUCCESS : If success \n
 *         USB_ERR_FAIL : If call fail \n
 */
int s5pc110_otghcd_urb_dequeue(
	struct usb_hcd *_hcd, struct urb *_urb, int status)
{
	int	ret_val = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(_hcd);

	unsigned long	spin_lock_flag = 0;
	td_t *cancel_td;

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_urb_dequeue \n");

	/* Dequeue should be performed only if endpoint is enabled */
	if (_urb->ep->enabled == 0) {
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		usb_hcd_giveback_urb(_hcd, _urb, status);
		return USB_ERR_SUCCESS;
	}

	//kevinh read this from inside the spinlock
	cancel_td = (td_t *)_urb->hcpriv;

	if (cancel_td == NULL) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "cancel_td is NULL\n");
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		return USB_ERR_FAIL;
	}
	otg_dbg(OTG_DBG_OTGHCDI_HCD,
		"s5pc110_otghcd_urb_dequeue, status = %d\n", status);


	ret_val = usb_hcd_check_unlink_urb(_hcd, _urb, status);
        if( (ret_val) && (ret_val != -EIDRM) ) {
		otg_dbg(OTG_DBG_OTGHCDI_HCD, "ret_val = %d\n", ret_val);
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		usb_hcd_giveback_urb(_hcd, _urb, status);
		return ret_val;
        }

	if (!HC_IS_RUNNING(_hcd->state)) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "!HC_IS_RUNNING(hcd->state) \n");
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		otg_usbcore_giveback(cancel_td);
		return USB_ERR_SUCCESS;
	}

	ret_val = cancel_transfer(otghost, cancel_td->parent_ed_p, cancel_td);
	if(ret_val != USB_ERR_DEQUEUED && ret_val != USB_ERR_NOELEMENT) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "fail to cancel_transfer() \n");
		otg_usbcore_giveback(cancel_td);
		spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
		return USB_ERR_FAIL;
	}
	otg_usbcore_giveback(cancel_td);
	delete_td(otghost, cancel_td);
	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
	return USB_ERR_SUCCESS;
}

/**
 * void s5pc110_otghcd_endpoint_disable(
 *		struct usb_hcd *hcd,
 *	       	struct usb_host_endpoint *ep)
 *
 * @brief  disable a endpoint
 *
 * @param  [in] hcd : pointer of usb_hcd
 *		   [in] ep : pointer of usb_host_endpoint
 */
void s5pc110_otghcd_endpoint_disable(
		struct usb_hcd *hcd,
	       	struct usb_host_endpoint *ep)
{
	int	ret_val = 0;
	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_endpoint_disable \n");

	if(!((ed_t *)ep->hcpriv))
		return;

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);
	ret_val = delete_ed(otghost, (ed_t *)ep->hcpriv);
	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);

	if(ret_val != USB_ERR_SUCCESS) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "fail to delete_ed() \n");
		return ;
	}

	/* ep->hcpriv = NULL; delete_ed coveres it */
}

/**
 * int s5pc110_otghcd_hub_status_data(struct usb_hcd *_hcd, char *_buf)
 *
 * @brief  get status of root hub
 *
 * @param  [in] _hcd : pointer of usb_hcd
 * 		   [inout] _buf : pointer of buffer for write a status data
 *
 * @return ret_val : return port status \n
 */
int s5pc110_otghcd_hub_status_data(struct usb_hcd *_hcd, char *_buf)
{
	int ret_val = 0;
	unsigned long spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(_hcd);

	/* otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_hub_status_data \n"); */

	/* if !USB_SUSPEND, root hub timers won't get shut down ... */
	if (!HC_IS_RUNNING(_hcd->state)) {
		otg_dbg(OTG_DBG_OTGHCDI_HCD,
			"_hcd->state is NOT HC_IS_RUNNING \n");
		return 0;
	}

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);
	ret_val = get_otg_port_status(_hcd, OTG_PORT_NUMBER, _buf);
	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);

	return (int)ret_val;
}

/**
 * int s5pc110_otghcd_hub_control()
 *
 * @brief  control root hub
 *
 * @param  [in] hcd : pointer of usb_hcd
 *		   [in] typeReq : type of control request
 *		   [in] value : value
 *		   [in] index : index
 *		   [in] buf_p : pointer of urb
 *		   [in] length : type of gfp_t
 *
 * @return ret_val : return root_hub_feature \n
 */
int
s5pc110_otghcd_hub_control (
		struct usb_hcd *hcd,
		u16 	typeReq,
		u16 	value,
		u16 	index,
		char*	buf_p,
		u16 	length
)
{
	int	ret_val = 0;
	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_hub_control \n");

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);

	ret_val = root_hub_feature(hcd, OTG_PORT_NUMBER,
		       	typeReq, value, (void *)buf_p);

	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
	if(ret_val != USB_ERR_SUCCESS) {
		otg_err(OTG_DBG_OTGHCDI_HCD, "fail to root_hub_feature() \n");
		return ret_val;
	}
	return (int)ret_val;
}

/**
 * int s5pc110_otghcd_bus_suspend(struct usb_hcd *hcd)
 *
 * @brief  suspend otg hcd
 *
 * @param  [in] hcd : pointer of usb_hcd
 *
 * @return USB_ERR_SUCCESS \n
 */
int	s5pc110_otghcd_bus_suspend(struct usb_hcd *hcd)
{
	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_bus_suspend \n");

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);
	bus_suspend();
	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);

	return USB_ERR_SUCCESS;
}

/**
 * int s5pc110_otghcd_bus_resume(struct usb_hcd *hcd)
 *
 * @brief  resume otg hcd
 *
 * @param  [in] hcd : pointer of usb_hcd
 *
 * @return USB_ERR_SUCCESS \n
 */
int	s5pc110_otghcd_bus_resume(struct usb_hcd *hcd)
{
	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_bus_resume \n");

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);

	if(bus_resume(otghost) != USB_ERR_SUCCESS) {
		return USB_ERR_FAIL;
	}

	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);
	return USB_ERR_SUCCESS;
}

/**
 * int s5pc110_otghcd_start_port_reset(struct usb_hcd *hcd, unsigned port)
 *
 * @brief reset otg port
 *
 * @param  [in] hcd : pointer of usb_hcd
 *		   [in] port : number of port
 *
 * @return USB_ERR_SUCCESS : If success \n
 *         ret_val : If call fail \n
 */
int	s5pc110_otghcd_start_port_reset(struct usb_hcd *hcd, unsigned port)
{
	int	ret_val = 0;

	unsigned long	spin_lock_flag = 0;
	struct sec_otghost *otghost = hcd_to_sec_otghost(hcd);

	otg_dbg(OTG_DBG_OTGHCDI_HCD, "s5pc110_otghcd_start_port_reset \n");

	spin_lock_irq_save_otg(&otghost->lock, spin_lock_flag);
	ret_val = reset_and_enable_port(hcd, OTG_PORT_NUMBER);
	spin_unlock_irq_save_otg(&otghost->lock, spin_lock_flag);

	if(ret_val != USB_ERR_SUCCESS) {
		otg_err(OTG_DBG_OTGHCDI_HCD,
			       	"fail to reset_and_enable_port() \n");
		return ret_val;
	}
	return USB_ERR_SUCCESS;
}
