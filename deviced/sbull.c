

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/timer.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/bio.h>

#include <linux/device-mapper.h>

MODULE_LICENSE("GPL");

static int sbull_major = 0;
module_param(sbull_major, int, 0);

static int hardsect_size = 512;
module_param(hardsect_size, int, 0);

static int nsectors = 1024;
module_param(nsectors, int, 0);

static int ndevices = 4;
module_param(ndevices, int, 0);


/*
 * 
 * */
enum {
	RM_SIMPLE = 0,
	RM_FULL = 1,
	RM_NOQUEUE = 2,

};


static int request_mode = RM_SIMPLE;
module_param(request_mode, int, 0);

#define SBULL_MINORS 16
#define MINOR_SHIFT 4
#define DENUM(kdevnum) (MINOR (kdev_t_to_nr(kdevnum) ) ) >> MINOR_SHIFT


#define KERNEL_SECTOR_SIZE 512

#define INVALIDATE_DELAY 39*HZ


struct sbull_dev {
	int size;
	u8* data;
	short users;
	short media_change;
	spinlock_t lock;
	struct request_queue *queue;
	struct gendisk *gd;
	struct timer_list timer;

};

static struct sbull_dev* devices = NULL;


static int sbull_open(struct block_device* bdev, fmode_t mode)
{
	struct sbull_dev* dev = bdev->bd_disk->private_data;

	printk(KERN_NOTICE "sbull open\n");
	del_timer_sync(&dev->timer);
	
	spin_lock(&dev->lock);
	dev->users++;
	spin_unlock(&dev->lock);
 	

	return 0;
}

static void sbull_release(struct gendisk* disk, fmode_t mode)
{
	struct sbull_dev* dev = disk->private_data;

	printk(KERN_NOTICE "sbull release\n");
	spin_lock(&dev->lock);
	dev->users--;

	if ( !dev->users ) {
		dev->timer.expires = jiffies + INVALIDATE_DELAY;
		add_timer(&dev->timer);	
	}
	spin_unlock(&dev->lock);

}


static void sbull_transfer(struct sbull_dev* dev, unsigned long  sector,
		unsigned long nsect, char* buffer, int write)
{
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;

	if ( (offset + nbytes) > dev->size ) {
		printk (KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
	}

	printk(KERN_NOTICE "For write %d? pos (%ld %ld)\n", write, offset, nbytes);
	if (write) {

		memcpy(dev->data + offset, buffer, nbytes);
	}
	else {
		memcpy(buffer, dev->data + offset, nbytes);
	}

}

int sbull_media_changed(struct gendisk* gd)
{
	struct sbull_dev* dev = gd->private_data;

	return dev->media_change;
}

int sbull_revalidate(struct gendisk* gd)
{
	struct sbull_dev* dev = gd->private_data;

	if (dev->media_change)
	{
		dev->media_change = 0;
		memset(dev->data, 0, dev->size);
	}

	return 0;
}


void sbull_invalidate(unsigned long ldev)
{
	struct sbull_dev* dev = (struct sbull_dev*) ldev;

	spin_lock(&dev->lock);
	if (dev->users || !dev->data)
		printk(KERN_WARNING "sbull: timer sanity check failed\n");
	else {
		dev->media_change = 1;
		printk(KERN_NOTICE "sbull: media change");
	}
	spin_unlock(&dev->lock);
}

int sbull_ioctl(struct block_device* bdev, fmode_t mode,
		unsigned cmd, unsigned long arg)
{
	return 0;

}


static int sbull_xfer_bio(struct sbull_dev* dev, struct bio* bio)
{
        struct bvec_iter i;	
	struct bio_vec bvec;
	sector_t sector = bio->bi_iter.bi_sector;

	bio_for_each_segment(bvec, bio, i) {
		char* buffer = __bio_kmap_atomic(bio, i);
		sbull_transfer(dev, sector, bvec.bv_len >> SECTOR_SHIFT,
				buffer, bio_data_dir(bio) == WRITE);
		sector += bvec.bv_len >> SECTOR_SHIFT;
		__bio_kunmap_atomic(bio);
	
	}
	return 0;
}

static void sbull_full_request(struct request_queue* q)
{
}


static void sbull_make_request(struct request_queue* q, struct bio* bio)
{

}


static void sbull_request(struct request_queue* q)
{
	struct request* req;
	struct bio* bio;
	printk(KERN_NOTICE "sbull new request\n");

	while ( (req = blk_peek_request(q) ) != NULL ) {
		struct sbull_dev* dev = req->rq_disk->private_data;
		if ( req->cmd_type != REQ_TYPE_FS ) {
			printk(KERN_NOTICE "Skip non-fs request \n");
			//sbull_transfer(dev, blk_rq_pos(req), blk_rq_sectors(req), req->buffer, rq_data_dir(req));
			//sbull_xfer_bio(dev, req->bio);
			blk_start_request(req);
			__blk_end_request_all(req, -EIO);
			continue;
		}

		blk_start_request(req);

		__rq_for_each_bio(bio, req)
			sbull_xfer_bio(dev, bio);

		__blk_end_request_all(req, 0);

	
	}


}


static struct block_device_operations sbull_ops = {
	.owner		= THIS_MODULE,
	.open		= sbull_open,
	.release	= sbull_release,
	.media_changed	= sbull_media_changed,
	.revalidate_disk	= sbull_revalidate,
	.ioctl		= sbull_ioctl

};


static void setup_device(struct sbull_dev* dev, int which)
{
	memset(dev, 0, sizeof(struct sbull_dev));
	dev->size = nsectors * hardsect_size;
	dev->data = vmalloc(dev->size);
	if (dev->data == NULL)
	{
		printk(KERN_NOTICE "vmalloc failed. \n");
		return;	
	}

	spin_lock_init(&dev->lock);

	init_timer(&dev->timer);
	dev->timer.data = (unsigned long)dev;
	dev->timer.function = sbull_invalidate;


	switch (request_mode) {
		case RM_NOQUEUE:
			dev->queue = blk_alloc_queue(GFP_KERNEL);
			if (dev->queue == NULL)
			{
				goto out_vfree;			
			}
			blk_queue_make_request(dev->queue, sbull_make_request);
			break;
		case RM_FULL:
			dev->queue = blk_init_queue(sbull_full_request, &dev->lock);
			if (dev->queue == NULL)
			{
				goto out_vfree;
			}
			break;

		default:
			printk(KERN_NOTICE "Bad request mode %d, using simple.\n", request_mode);

		case RM_SIMPLE:
			dev->queue = blk_init_queue(sbull_request, &dev->lock);
			if (dev->queue == NULL)
			{
				goto out_vfree;
			}
			break;
	
	}

//	blk_queue_hardsect_size(dev->queue, hardsect_size);
	//dev->queue->hardsect_size = hardsect_size;
	dev->queue->queuedata = dev;
	blk_queue_logical_block_size(dev->queue, hardsect_size);

	dev->gd = alloc_disk(SBULL_MINORS);
	if ( !dev->gd ) {
		printk(KERN_NOTICE "alloc_disk failure\n");
		goto out_vfree;
	
	}

	dev->gd->major = sbull_major;
	dev->gd->first_minor = which*SBULL_MINORS;
	dev->gd->fops = &sbull_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf( dev->gd->disk_name, 32, "sbull%c", ('a' + which));
	printk(KERN_NOTICE "Disk name %s\n", dev->gd->disk_name);
	set_capacity( dev->gd, nsectors*(hardsect_size/KERNEL_SECTOR_SIZE) );
	add_disk(dev->gd);
	return;


out_vfree:
	if (dev->data)
	{
		vfree(dev->data);	
	}
}


static int __init sbull_init(void)
{
	int i;

	sbull_major = register_blkdev(sbull_major, "sbull");
	if (sbull_major <= 0)
	{
		printk(KERN_WARNING "sbull: unable to get major number\n");
		return -EBUSY;
	}
	
	devices = kmalloc(ndevices*sizeof(struct sbull_dev), GFP_KERNEL);
	if (devices == NULL)
	{
		goto out_unregister;
	}

	for (i = 0; i < ndevices; i++)
	{
		setup_device(devices + i, i);
	}
	
	return 0;

out_unregister:
	unregister_blkdev(sbull_major, "sbull");
	return -ENOMEM;

}

static void sbull_exit(void)
{
	int i;

	for (i = 0; i < ndevices; i++)
	{
		struct sbull_dev* dev = devices + i;

		del_timer_sync(&dev->timer);
		if (dev->gd)
		{
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}

		if (dev->queue) {
			if (request_mode == RM_NOQUEUE) {
				blk_put_queue(dev->queue);
			}
			else {
				blk_cleanup_queue(dev->queue);
			}
		
		}

		if (dev->data) {
			vfree(dev->data);
		}
	
	}

	unregister_blkdev(sbull_major, "sbull");
	kfree(devices);
}

module_init(sbull_init);
module_exit(sbull_exit);
