/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/kobject.h>

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <aee.h>

/* Define SMI_INTERNAL_CCF_SUPPORT when CCF needs to be enabled */
#if !defined(CONFIG_MTK_CLKMGR)
#define SMI_INTERNAL_CCF_SUPPORT
#endif

#if defined(SMI_INTERNAL_CCF_SUPPORT)
#include <linux/clk.h>
/* for ccf clk CB */
#if defined(SMI_D1)
#include "clk-mt6735-pg.h"
#elif defined(SMI_J)
#include "clk-mt6755-pg.h"
#endif
/* notify clk is enabled/disabled for m4u*/
#include "m4u.h"
#else
#include <mach/mt_clkmgr.h>
#endif				/* defined(SMI_INTERNAL_CCF_SUPPORT) */

#include <asm/io.h>

#include <linux/ioctl.h>
#include <linux/fs.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

#include <mt_smi.h>


#include "smi_reg.h"
#include "smi_common.h"
#include "smi_debug.h"
#include "smi_info_util.h"
#include "smi_configuration.h"
#if defined(SMI_D1) || defined(SMI_D2) || defined(SMI_D3)
#include "mmdvfs_mgr.h"
#endif
#undef pr_fmt
#define pr_fmt(fmt) "[SMI]" fmt

#define SMI_LOG_TAG "SMI"

#define LARB_BACKUP_REG_SIZE 128
#define SMI_COMMON_BACKUP_REG_NUM   7

#define SF_HWC_PIXEL_MAX_NORMAL  (1920 * 1080 * 7)
#define SF_HWC_PIXEL_MAX_VR   (1920 * 1080 * 4 + 1036800)	/* 4.5 FHD size */
#define SF_HWC_PIXEL_MAX_VP   (1920 * 1080 * 7)
#define SF_HWC_PIXEL_MAX_ALWAYS_GPU  (1920 * 1080 * 1)

/* debug level */
static unsigned int smi_debug_level;

#define SMIDBG(level, x...)            \
		do {                        \
			if (smi_debug_level >= (level))    \
				SMIMSG(x);            \
		} while (0)

#define DEFINE_ATTR_RO(_name)\
			static struct kobj_attribute _name##_attr = {\
				.attr	= {\
					.name = #_name,\
					.mode = 0444,\
				},\
				.show	= _name##_show,\
			}

#define DEFINE_ATTR_RW(_name)\
			static struct kobj_attribute _name##_attr = {\
				.attr	= {\
					.name = #_name,\
					.mode = 0644,\
				},\
				.show	= _name##_show,\
				.store	= _name##_store,\
			}

#define __ATTR_OF(_name)	(&_name##_attr.attr)

struct SMI_struct {
	spinlock_t SMI_lock;
	unsigned int pu4ConcurrencyTable[SMI_BWC_SCEN_CNT];	/* one bit represent one module */
};

static struct SMI_struct g_SMIInfo;

/* LARB BASE ADDRESS */
unsigned long gLarbBaseAddr[SMI_LARB_NR] = { 0 };

/* DT porting */
unsigned long smi_reg_base_common_ext = 0;
unsigned long smi_reg_base_barb0 = 0;
unsigned long smi_reg_base_barb1 = 0;
unsigned long smi_reg_base_barb2 = 0;
unsigned long smi_reg_base_barb3 = 0;




char *smi_get_region_name(unsigned int region_indx);


static struct smi_device *smi_dev;

static struct device *smiDeviceUevent;

static struct cdev *pSmiDev;

#define SMI_COMMON_REG_INDX 0
#define SMI_LARB0_REG_INDX 1
#define SMI_LARB1_REG_INDX 2
#define SMI_LARB2_REG_INDX 3
#define SMI_LARB3_REG_INDX 4

#if defined(SMI_D2)
#define SMI_REG_REGION_MAX 4

static const unsigned int larb_port_num[SMI_LARB_NR] = { SMI_LARB0_PORT_NUM,
	SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM
};

static unsigned char larb_vc_setting[SMI_LARB_NR] = { 0, 2, 1 };

static unsigned short int larb0_port_backup[SMI_LARB0_PORT_NUM];
static unsigned short int larb1_port_backup[SMI_LARB1_PORT_NUM];
static unsigned short int larb2_port_backup[SMI_LARB2_PORT_NUM];

static unsigned short int *larb_port_backup[SMI_LARB_NR] = {
	larb0_port_backup, larb1_port_backup, larb2_port_backup
};


#elif defined(SMI_D1)
#define SMI_REG_REGION_MAX 5

static const unsigned int larb_port_num[SMI_LARB_NR] = { SMI_LARB0_PORT_NUM,
	SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM
};

static unsigned char larb_vc_setting[SMI_LARB_NR] = { 0, 2, 0, 1 };

static unsigned short int larb0_port_backup[SMI_LARB0_PORT_NUM];
static unsigned short int larb1_port_backup[SMI_LARB1_PORT_NUM];
static unsigned short int larb2_port_backup[SMI_LARB2_PORT_NUM];
static unsigned short int larb3_port_backup[SMI_LARB3_PORT_NUM];
static unsigned short int *larb_port_backup[SMI_LARB_NR] = {
	larb0_port_backup, larb1_port_backup, larb2_port_backup, larb3_port_backup
};


#elif defined(SMI_D3)
#define SMI_REG_REGION_MAX 5

static const unsigned int larb_port_num[SMI_LARB_NR] = { SMI_LARB0_PORT_NUM,
	SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM
};

static unsigned short int larb0_port_backup[SMI_LARB0_PORT_NUM];
static unsigned short int larb1_port_backup[SMI_LARB1_PORT_NUM];
static unsigned short int larb2_port_backup[SMI_LARB2_PORT_NUM];
static unsigned short int larb3_port_backup[SMI_LARB3_PORT_NUM];

static unsigned char larb_vc_setting[SMI_LARB_NR] = { 0, 2, 1, 1 };

static unsigned short int *larb_port_backup[SMI_LARB_NR] = {
	larb0_port_backup, larb1_port_backup, larb2_port_backup, larb3_port_backup
};
#elif defined(SMI_R)

#define SMI_REG_REGION_MAX 3

static const unsigned int larb_port_num[SMI_LARB_NR] = { SMI_LARB0_PORT_NUM,
	SMI_LARB1_PORT_NUM
};

static unsigned short int larb0_port_backup[SMI_LARB0_PORT_NUM];
static unsigned short int larb1_port_backup[SMI_LARB1_PORT_NUM];

static unsigned char larb_vc_setting[SMI_LARB_NR] = { 0, 2 };

static unsigned short int *larb_port_backup[SMI_LARB_NR] = {
	larb0_port_backup, larb1_port_backup
};

#elif defined(SMI_J)
#define SMI_REG_REGION_MAX 5


static const unsigned int larb_port_num[SMI_LARB_NR] = { SMI_LARB0_PORT_NUM,
	SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM
};

static unsigned short int larb0_port_backup[SMI_LARB0_PORT_NUM];
static unsigned short int larb1_port_backup[SMI_LARB1_PORT_NUM];
static unsigned short int larb2_port_backup[SMI_LARB2_PORT_NUM];
static unsigned short int larb3_port_backup[SMI_LARB3_PORT_NUM];

static unsigned char larb_vc_setting[SMI_LARB_NR] = { 1, 2, 1, 1 };

static unsigned short int *larb_port_backup[SMI_LARB_NR] = { larb0_port_backup,
	larb1_port_backup, larb2_port_backup, larb3_port_backup
};
#endif

static unsigned long gSMIBaseAddrs[SMI_REG_REGION_MAX];

/* SMI COMMON register list to be backuped */
static unsigned short g_smi_common_backup_reg_offset[SMI_COMMON_BACKUP_REG_NUM] = { 0x100, 0x104,
	0x108, 0x10c, 0x110, 0x230, 0x234
};

static unsigned int g_smi_common_backup[SMI_COMMON_BACKUP_REG_NUM];
struct smi_device {
	struct device *dev;
	void __iomem *regs[SMI_REG_REGION_MAX];
#if defined(SMI_INTERNAL_CCF_SUPPORT)
	struct clk *smi_common_clk;
	struct clk *smi_larb0_clk;
	struct clk *img_larb2_clk;
	struct clk *vdec0_vdec_clk;
	struct clk *vdec1_larb_clk;
	struct clk *venc_larb_clk;
	struct clk *venc_venc_clk;
	struct clk *larb0_mtcmos;
	struct clk *larb1_mtcmos;
	struct clk *larb2_mtcmos;
	struct clk *larb3_mtcmos;
#endif
};


/* To keep the HW's init value */
static int is_default_value_saved;
static unsigned int default_val_smi_l1arb[SMI_LARB_NR] = { 0 };

static unsigned int wifi_disp_transaction;


/* larb backuprestore */
#if defined(SMI_INTERNAL_CCF_SUPPORT)
static bool fglarbcallback;
#endif
/* tuning mode, 1 for register ioctl */
static unsigned int smi_tuning_mode;
#if defined(SMI_J)
static unsigned int disable_freq_hopping;
static unsigned int disable_freq_mux = 1;
static unsigned int force_max_mmsys_clk;
static unsigned int force_camera_hpm;
#endif
static unsigned int smi_profile = SMI_BWC_SCEN_NORMAL;

static unsigned int *pLarbRegBackUp[SMI_LARB_NR];
static int g_bInited;

MTK_SMI_BWC_MM_INFO g_smi_bwc_mm_info = {
	0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 0, 0, 0,
	SF_HWC_PIXEL_MAX_NORMAL
};

char *smi_port_name[][21] = {
	{			/* 0 MMSYS */
	 "disp_ovl0", "disp_rdma0", "disp_rdma1", "disp_wdma0", "disp_ovl1",
	 "disp_rdma2", "disp_wdma1", "disp_od_r", "disp_od_w", "mdp_rdma0",
	 "mdp_rdma1", "mdp_wdma", "mdp_wrot0", "mdp_wrot1"},
	{ /* 1 VDEC */ "hw_vdec_mc_ext", "hw_vdec_pp_ext", "hw_vdec_ufo_ext", "hw_vdec_vld_ext",
	 "hw_vdec_vld2_ext", "hw_vdec_avc_mv_ext", "hw_vdec_pred_rd_ext",
	 "hw_vdec_pred_wr_ext", "hw_vdec_ppwrap_ext"},
	{ /* 2 ISP */ "imgo", "rrzo", "aao", "lcso", "esfko", "imgo_d", "lsci", "lsci_d", "bpci",
	 "bpci_d", "ufdi", "imgi", "img2o", "img3o", "vipi", "vip2i", "vip3i",
	 "lcei", "rb", "rp", "wr"},
	{ /* 3 VENC */ "venc_rcpu", "venc_rec", "venc_bsdma", "venc_sv_comv", "venc_rd_comv",
	 "jpgenc_bsdma", "remdc_sdma", "remdc_bsdma", "jpgenc_rdma", "jpgenc_sdma",
	 "jpgdec_wdma", "jpgdec_bsdma", "venc_cur_luma", "venc_cur_chroma",
	 "venc_ref_luma", "venc_ref_chroma", "remdc_wdma", "venc_nbm_rdma",
	 "venc_nbm_wdma"},
	{ /* 4 MJC */ "mjc_mv_rd", "mjc_mv_wr", "mjc_dma_rd", "mjc_dma_wr"}
};



static unsigned long get_register_base(int i);


#if defined(SMI_INTERNAL_CCF_SUPPORT)
static struct clk *get_smi_clk(char *smi_clk_name);
#endif

#if IS_ENABLED(CONFIG_COMPAT)
static long MTK_SMI_COMPAT_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
#define MTK_SMI_COMPAT_ioctl  NULL
#endif


/* Use this function to get base address of Larb resgister */
/* to support error checking */
unsigned long get_larb_base_addr(int larb_id)
{
	if (larb_id >= SMI_LARB_NR || larb_id < 0)
		return SMI_ERROR_ADDR;
	else
		return gLarbBaseAddr[larb_id];

}

/* 0 for common, 1 for larb0, 2 for larb1... */
unsigned long get_smi_base_addr(int larb_id)
{
	if (larb_id >= SMI_LARB_NR || larb_id < 0)
		return SMI_ERROR_ADDR;
	else
		return gSMIBaseAddrs[larb_id];
}
EXPORT_SYMBOL(get_smi_base_addr);


#if defined(SMI_INTERNAL_CCF_SUPPORT)
struct clk *get_smi_clk(char *smi_clk_name)
{
	struct clk *smi_clk_ptr = NULL;

	smi_clk_ptr = devm_clk_get(smi_dev->dev, smi_clk_name);
	if (IS_ERR(smi_clk_ptr)) {
		SMIMSG("cannot get %s\n", smi_clk_name);
		smi_clk_ptr = NULL;
	}
	return smi_clk_ptr;
}

static void smi_prepare_clk(struct clk *smi_clk, char *name)
{
	if (smi_clk != NULL) {
		int ret = 0;

		ret = clk_prepare(smi_clk);
		if (ret)
			SMIMSG("clk_prepare return error %d, %s\n", ret, name);
	} else {
		SMIMSG("clk_prepare error, smi_clk can't be NULL, %s\n", name);
	}
}

static void smi_enable_clk(struct clk *smi_clk, char *name)
{
	if (smi_clk != NULL) {
		int ret = 0;

		ret = clk_enable(smi_clk);
		if (ret)
			SMIMSG("clk_enable return error %d, %s\n", ret, name);
	} else {
		SMIMSG("clk_enable error, smi_clk can't be NULL, %s\n", name);
	}
}

static void smi_unprepare_clk(struct clk *smi_clk, char *name)
{
	if (smi_clk != NULL)
		clk_unprepare(smi_clk);
	else
		SMIMSG("smi_unprepare error, smi_clk can't be NULL, %s\n", name);
}

static void smi_disable_clk(struct clk *smi_clk, char *name)
{
	if (smi_clk != NULL)
		clk_disable(smi_clk);
	else
		SMIMSG("smi_disable error, smi_clk can't be NULL, %s\n", name);
}

/* end MTCMOS*/
#endif				/* defined(SMI_INTERNAL_CCF_SUPPORT) */

static int larb_clock_enable(int larb_id, int enable_mtcmos)
{
#if !defined(CONFIG_MTK_FPGA) && !defined(CONFIG_FPGA_EARLY_PORTING)
	char name[30];

	sprintf(name, "smi+%d", larb_id);


	switch (larb_id) {
#if !defined(SMI_INTERNAL_CCF_SUPPORT)
	case 0:
		enable_clock(MT_CG_DISP0_SMI_COMMON, name);
		enable_clock(MT_CG_DISP0_SMI_LARB0, name);
		break;
	case 1:
		enable_clock(MT_CG_DISP0_SMI_COMMON, name);
#if defined(SMI_R)
		enable_clock(MT_CG_LARB1_SMI_CKPDN, name);
#else
		enable_clock(MT_CG_VDEC1_LARB, name);
#endif
		break;
	case 2:
#if !defined(SMI_R)
		enable_clock(MT_CG_DISP0_SMI_COMMON, name);
		enable_clock(MT_CG_IMAGE_LARB2_SMI, name);
#endif
		break;
	case 3:
		enable_clock(MT_CG_DISP0_SMI_COMMON, name);
#if defined(SMI_D1)
		enable_clock(MT_CG_VENC_LARB, name);
#elif defined(SMI_D3)
		enable_clock(MT_CG_VENC_VENC, name);
#endif
		break;
#else
	case 0:
		if (enable_mtcmos)
			smi_enable_clk(smi_dev->larb0_mtcmos, name);
		smi_enable_clk(smi_dev->smi_common_clk, name);
		smi_enable_clk(smi_dev->smi_larb0_clk, name);
		break;
	case 1:
		if (enable_mtcmos) {
			smi_enable_clk(smi_dev->larb0_mtcmos, name);
			smi_enable_clk(smi_dev->larb1_mtcmos, name);
		}
		smi_enable_clk(smi_dev->smi_common_clk, name);
		smi_enable_clk(smi_dev->vdec1_larb_clk, name);
		break;
	case 2:
		if (enable_mtcmos) {
			smi_enable_clk(smi_dev->larb0_mtcmos, name);
			smi_enable_clk(smi_dev->larb2_mtcmos, name);
		}
		smi_enable_clk(smi_dev->smi_common_clk, name);
		smi_enable_clk(smi_dev->img_larb2_clk, name);
		break;
	case 3:
		if (enable_mtcmos) {
			smi_enable_clk(smi_dev->larb0_mtcmos, name);
			smi_enable_clk(smi_dev->larb3_mtcmos, name);
		}
		smi_enable_clk(smi_dev->smi_common_clk, name);
#if defined(SMI_D1)
		smi_enable_clk(smi_dev->venc_larb_clk, name);
#elif defined(SMI_J)
		smi_enable_clk(smi_dev->venc_venc_clk, name);
#endif
		break;
#endif
	default:
		break;
	}
#endif
	return 0;
}

static int larb_clock_prepare(int larb_id, int enable_mtcmos)
{
#if !defined(CONFIG_MTK_FPGA) && !defined(CONFIG_FPGA_EARLY_PORTING) && defined(SMI_INTERNAL_CCF_SUPPORT)
	char name[30];

	sprintf(name, "smi+%d", larb_id);

	switch (larb_id) {
	case 0:
		/* must enable MTCOMS before clk */
		/* common MTCMOS is called with larb0_MTCMOS */
		if (enable_mtcmos)
			smi_prepare_clk(smi_dev->larb0_mtcmos, name);
		smi_prepare_clk(smi_dev->smi_common_clk, name);
		smi_prepare_clk(smi_dev->smi_larb0_clk, name);
		break;
	case 1:
		if (enable_mtcmos) {
			smi_prepare_clk(smi_dev->larb0_mtcmos, name);
			smi_prepare_clk(smi_dev->larb1_mtcmos, name);
		}
		smi_prepare_clk(smi_dev->smi_common_clk, name);
		smi_prepare_clk(smi_dev->vdec1_larb_clk, name);
		break;
	case 2:
		if (enable_mtcmos) {
			smi_prepare_clk(smi_dev->larb0_mtcmos, name);
			smi_prepare_clk(smi_dev->larb2_mtcmos, name);
		}
		smi_prepare_clk(smi_dev->smi_common_clk, name);
		smi_prepare_clk(smi_dev->img_larb2_clk, name);
		break;
	case 3:
		if (enable_mtcmos) {
			smi_prepare_clk(smi_dev->larb0_mtcmos, name);
			smi_prepare_clk(smi_dev->larb3_mtcmos, name);
		}
		smi_prepare_clk(smi_dev->smi_common_clk, name);
#if defined(SMI_D1)
		smi_prepare_clk(smi_dev->venc_larb_clk, name);
#elif defined(SMI_J)
		smi_prepare_clk(smi_dev->venc_venc_clk, name);
#endif
		break;
	default:
		break;
	}
#endif
	return 0;
}

static int larb_clock_disable(int larb_id, int enable_mtcmos)
{
#if !defined(CONFIG_MTK_FPGA) && !defined(CONFIG_FPGA_EARLY_PORTING)
	char name[30];

	sprintf(name, "smi+%d", larb_id);

	switch (larb_id) {
#if !defined(SMI_INTERNAL_CCF_SUPPORT)
	case 0:
		disable_clock(MT_CG_DISP0_SMI_LARB0, name);
		disable_clock(MT_CG_DISP0_SMI_COMMON, name);
		break;
	case 1:
#if defined(SMI_R)
		disable_clock(MT_CG_LARB1_SMI_CKPDN, name);
#else
		disable_clock(MT_CG_VDEC1_LARB, name);
#endif
		disable_clock(MT_CG_DISP0_SMI_COMMON, name);
		break;
	case 2:
#if !defined(SMI_R)
		disable_clock(MT_CG_IMAGE_LARB2_SMI, name);
		disable_clock(MT_CG_DISP0_SMI_COMMON, name);
#endif
		break;
	case 3:
#if defined(SMI_D1)
		disable_clock(MT_CG_VENC_LARB, name);
#elif defined(SMI_D3)
		disable_clock(MT_CG_VENC_VENC, name);
#endif
		disable_clock(MT_CG_DISP0_SMI_COMMON, name);
		break;
#else
	case 0:
		smi_disable_clk(smi_dev->smi_larb0_clk, name);
		smi_disable_clk(smi_dev->smi_common_clk, name);
		if (enable_mtcmos)
			smi_disable_clk(smi_dev->larb0_mtcmos, name);
		break;
	case 1:
		smi_disable_clk(smi_dev->vdec1_larb_clk, name);
		smi_disable_clk(smi_dev->smi_common_clk, name);
		if (enable_mtcmos) {
			smi_disable_clk(smi_dev->larb1_mtcmos, name);
			smi_disable_clk(smi_dev->larb0_mtcmos, name);
		}
		break;
	case 2:
		smi_disable_clk(smi_dev->img_larb2_clk, name);
		smi_disable_clk(smi_dev->smi_common_clk, name);
		if (enable_mtcmos) {
			smi_disable_clk(smi_dev->larb2_mtcmos, name);
			smi_disable_clk(smi_dev->larb0_mtcmos, name);
		}
		break;
	case 3:
#if defined(SMI_D1)
		smi_disable_clk(smi_dev->venc_larb_clk, name);
#elif defined(SMI_J)
		smi_disable_clk(smi_dev->venc_venc_clk, name);
#endif
		smi_disable_clk(smi_dev->smi_common_clk, name);
		if (enable_mtcmos) {
			smi_disable_clk(smi_dev->larb3_mtcmos, name);
			smi_disable_clk(smi_dev->larb0_mtcmos, name);
		}
		break;
#endif
	default:
		break;
	}
#endif
	return 0;
}

static int larb_clock_unprepare(int larb_id, int enable_mtcmos)
{
#if !defined(CONFIG_MTK_FPGA) && !defined(CONFIG_FPGA_EARLY_PORTING) && defined(SMI_INTERNAL_CCF_SUPPORT)
	char name[30];

	sprintf(name, "smi+%d", larb_id);

	switch (larb_id) {
	case 0:
		/* must enable MTCOMS before clk */
		/* common MTCMOS is called with larb0_MTCMOS */
		smi_unprepare_clk(smi_dev->smi_larb0_clk, name);
		smi_unprepare_clk(smi_dev->smi_common_clk, name);
		if (enable_mtcmos)
			smi_unprepare_clk(smi_dev->larb0_mtcmos, name);
		break;
	case 1:
		smi_unprepare_clk(smi_dev->vdec1_larb_clk, name);
		smi_unprepare_clk(smi_dev->smi_common_clk, name);
		if (enable_mtcmos) {
			smi_unprepare_clk(smi_dev->larb1_mtcmos, name);
			smi_unprepare_clk(smi_dev->larb0_mtcmos, name);
		}
		break;
	case 2:
		smi_unprepare_clk(smi_dev->img_larb2_clk, name);
		smi_unprepare_clk(smi_dev->smi_common_clk, name);
		if (enable_mtcmos) {
			smi_unprepare_clk(smi_dev->larb2_mtcmos, name);
			smi_unprepare_clk(smi_dev->larb0_mtcmos, name);
		}
		break;
	case 3:
#if defined(SMI_D1)
		smi_unprepare_clk(smi_dev->venc_larb_clk, name);
#elif defined(SMI_J)
		smi_unprepare_clk(smi_dev->venc_venc_clk, name);
#endif
		smi_unprepare_clk(smi_dev->smi_common_clk, name);
		if (enable_mtcmos) {
			smi_unprepare_clk(smi_dev->larb3_mtcmos, name);
			smi_unprepare_clk(smi_dev->larb0_mtcmos, name);
		}
		break;

	default:
		break;
	}
#endif
	return 0;
}

static void backup_smi_common(void)
{
	int i;

	for (i = 0; i < SMI_COMMON_BACKUP_REG_NUM; i++) {
		g_smi_common_backup[i] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, (unsigned long)
						       g_smi_common_backup_reg_offset[i]);
	}
}

static void restore_smi_common(void)
{
	int i;

	for (i = 0; i < SMI_COMMON_BACKUP_REG_NUM; i++) {
		M4U_WriteReg32(SMI_COMMON_EXT_BASE,
			       (unsigned long)g_smi_common_backup_reg_offset[i],
			       g_smi_common_backup[i]);
	}
}

static void backup_larb_smi(int index)
{
	int port_index = 0;
	unsigned short int *backup_ptr = NULL;
	unsigned long larb_base = 0;
	unsigned long larb_offset = 0x200;
	int total_port_num = 0;

	/* boundary check for larb_port_num and larb_port_backup access */
	if (index < 0 || index >= SMI_LARB_NR)
		return;

	larb_base = gLarbBaseAddr[index];
	total_port_num = larb_port_num[index];
	backup_ptr = larb_port_backup[index];

	/* boundary check for port value access */
	if (total_port_num <= 0 || backup_ptr == NULL)
		return;

	for (port_index = 0; port_index < total_port_num; port_index++) {
		*backup_ptr = (unsigned short int)(M4U_ReadReg32(larb_base, larb_offset));
		backup_ptr++;
		larb_offset += 4;
	}

	/* backup smi common along with larb0, smi common clk is guaranteed to be on when processing larbs */
	if (index == 0)
		backup_smi_common();

}

static void restore_larb_smi(int index)
{
	int port_index = 0;
	unsigned short int *backup_ptr = NULL;
	unsigned long larb_base = 0;
	unsigned long larb_offset = 0x200;
	unsigned int backup_value = 0;
	int total_port_num = 0;

	/* boundary check for larb_port_num and larb_port_backup access */
	if (index < 0 || index >= SMI_LARB_NR)
		return;

	larb_base = gLarbBaseAddr[index];
	total_port_num = larb_port_num[index];
	backup_ptr = larb_port_backup[index];

	/* boundary check for port value access */
	if (total_port_num <= 0 || backup_ptr == NULL)
		return;

	/* restore smi common along with larb0, smi common clk is guaranteed to be on when processing larbs */
	if (index == 0)
		restore_smi_common();

	for (port_index = 0; port_index < total_port_num; port_index++) {
		backup_value = *backup_ptr;
		M4U_WriteReg32(larb_base, larb_offset, backup_value);
		backup_ptr++;
		larb_offset += 4;
	}

	/* we do not backup 0x20 because it is a fixed setting */
	M4U_WriteReg32(larb_base, 0x20, larb_vc_setting[index]);

	/* turn off EMI empty OSTD dobule, fixed setting */
	M4U_WriteReg32(larb_base, 0x2c, 4);

}

static int larb_reg_backup(int larb)
{
	unsigned int *pReg = pLarbRegBackUp[larb];
	unsigned long larb_base = gLarbBaseAddr[larb];

	*(pReg++) = M4U_ReadReg32(larb_base, SMI_LARB_CON);

	backup_larb_smi(larb);

	if (0 == larb)
		g_bInited = 0;

	return 0;
}

static int smi_larb_init(unsigned int larb)
{
	unsigned int regval = 0;
	unsigned int regval1 = 0;
	unsigned int regval2 = 0;
	unsigned long larb_base = get_larb_base_addr(larb);

	/* Clock manager enable LARB clock before call back restore already,
	 * it will be disabled after restore call back returns
	 * Got to enable OSTD before engine starts */
	regval = M4U_ReadReg32(larb_base, SMI_LARB_STAT);

	/* TODO: FIX ME */
	/* regval1 = M4U_ReadReg32(larb_base , SMI_LARB_MON_BUS_REQ0); */
	/* regval2 = M4U_ReadReg32(larb_base , SMI_LARB_MON_BUS_REQ1); */

	if (0 == regval) {
		SMIDBG(1, "Init OSTD for larb_base: 0x%lx\n", larb_base);
		M4U_WriteReg32(larb_base, SMI_LARB_OSTDL_SOFT_EN, 0xffffffff);
	} else {
		SMIMSG("Larb: 0x%lx is busy : 0x%x , port:0x%x,0x%x ,fail to set OSTD\n",
		       larb_base, regval, regval1, regval2);
		if (smi_debug_level >= 1) {
			SMIERR("DISP_MDP LARB  0x%lx OSTD cannot be set:0x%x,port:0x%x,0x%x\n",
			       larb_base, regval, regval1, regval2);
		} else {
			dump_stack();
		}
	}

	restore_larb_smi(larb);

	return 0;
}

int larb_reg_restore(int larb)
{
	unsigned long larb_base = SMI_ERROR_ADDR;
	unsigned int regval = 0;
	unsigned int *pReg = NULL;

	larb_base = get_larb_base_addr(larb);

	/* The larb assign doesn't exist */
	if (larb_base == SMI_ERROR_ADDR) {
		SMIMSG("Can't find the base address for Larb%d\n", larb);
		return 0;
	}

	if (larb >= SMI_LARB_NR || larb < 0) {
		SMIMSG("Can't find the backup register value for Larb%d\n", larb);
		return 0;
	}

	pReg = pLarbRegBackUp[larb];

	SMIDBG(1, "+larb_reg_restore(), larb_idx=%d\n", larb);
	SMIDBG(1, "m4u part restore, larb_idx=%d\n", larb);
	/* warning: larb_con is controlled by set/clr */
	regval = *(pReg++);
	M4U_WriteReg32(larb_base, SMI_LARB_CON_CLR, ~(regval));
	M4U_WriteReg32(larb_base, SMI_LARB_CON_SET, (regval));

	smi_larb_init(larb);

	return 0;
}

/* callback after larb clock is enabled */
#if !defined(SMI_INTERNAL_CCF_SUPPORT)
void on_larb_power_on(struct larb_monitor *h, int larb_idx)
{
	larb_reg_restore(larb_idx);
}

/* callback before larb clock is disabled */
void on_larb_power_off(struct larb_monitor *h, int larb_idx)
{
	larb_reg_backup(larb_idx);
}
#endif				/* !defined(SMI_INTERNAL_CCF_SUPPORT) */

#if defined(SMI_INTERNAL_CCF_SUPPORT)
void on_larb_power_on_with_ccf(int larb_idx)
{
	/* MTCMOS has already enable, only enable clk here to set register value */
	larb_clock_prepare(larb_idx, 0);
	larb_clock_enable(larb_idx, 0);
	larb_reg_restore(larb_idx);
	larb_clock_disable(larb_idx, 0);
	larb_clock_unprepare(larb_idx, 0);
}

void on_larb_power_off_with_ccf(int larb_idx)
{
	/* enable clk here for get correct register value */
	larb_clock_prepare(larb_idx, 0);
	larb_clock_enable(larb_idx, 0);
	larb_reg_backup(larb_idx);
	larb_clock_disable(larb_idx, 0);
	larb_clock_unprepare(larb_idx, 0);
}
#endif				/* defined(SMI_INTERNAL_CCF_SUPPORT) */

#if defined(SMI_J)
static void DCM_enable(void)
{
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x300, 0x1 + (0x78 << 1) + (0x4 << 8));
	M4U_WriteReg32(LARB0_BASE, 0x14, (0x7 << 8) + (0xf << 4));
	M4U_WriteReg32(LARB1_BASE, 0x14, (0x7 << 8) + (0xf << 4));
	M4U_WriteReg32(LARB2_BASE, 0x14, (0x7 << 8) + (0xf << 4));
	M4U_WriteReg32(LARB3_BASE, 0x14, (0x7 << 8) + (0xf << 4));

}
#endif


/* Fake mode check, e.g. WFD */
static int fake_mode_handling(MTK_SMI_BWC_CONFIG *p_conf, unsigned int *pu4LocalCnt)
{
	if (p_conf->scenario == SMI_BWC_SCEN_WFD) {
		if (p_conf->b_on_off) {
			wifi_disp_transaction = 1;
			SMIMSG("Enable WFD in profile: %d\n", smi_profile);
		} else {
			wifi_disp_transaction = 0;
			SMIMSG("Disable WFD in profile: %d\n", smi_profile);
		}
		return 1;
	} else {
		return 0;
	}
}

static int ovl_limit_uevent(int bwc_scenario, int ovl_pixel_limit)
{
	int err = 0;
	char *envp[3];
	char scenario_buf[32] = "";
	char ovl_limit_buf[32] = "";

	snprintf(scenario_buf, 31, "SCEN=%d", bwc_scenario);
	snprintf(ovl_limit_buf, 31, "HWOVL=%d", ovl_pixel_limit);

	envp[0] = scenario_buf;
	envp[1] = ovl_limit_buf;
	envp[2] = NULL;

	if (pSmiDev != NULL) {
		err = kobject_uevent_env(&(smiDeviceUevent->kobj), KOBJ_CHANGE, envp);
		SMIMSG("Notify OVL limitaion=%d, SCEN=%d", ovl_pixel_limit, bwc_scenario);
	}

	if (err < 0)
		SMIMSG(KERN_INFO "[%s] kobject_uevent_env error = %d\n", __func__, err);

	return err;
}

#if defined(SMI_INTERNAL_CCF_SUPPORT)
static unsigned int smiclk_subsys_2_larb(enum subsys_id sys)
{
	unsigned int i4larbid = 0;

	switch (sys) {
	case SYS_DIS:
		i4larbid = 0;	/*0&4 is disp */
		break;
	case SYS_VDE:
		i4larbid = 1;
		break;
	case SYS_ISP:
		i4larbid = 2;
		break;
	case SYS_VEN:
		i4larbid = 3;
		break;
	default:
		i4larbid = SMI_LARB_NR;
		break;
	}
	return i4larbid;
}

static void smiclk_subsys_after_on(enum subsys_id sys)
{
	unsigned int i4larbid = smiclk_subsys_2_larb(sys);

	if (!fglarbcallback) {
		SMIDBG(1, "don't need restore incb\n");
		return;
	}

	if (i4larbid < SMI_LARB_NR) {
		on_larb_power_on_with_ccf(i4larbid);
#if defined(SMI_D1)
		/* inform m4u to restore register value */
		m4u_larb_backup((int)i4larbid);
#endif
	} else {
		SMIDBG(1, "subsys id don't backup sys %d larb %u\n", sys, i4larbid);
	}
}

static void smiclk_subsys_before_off(enum subsys_id sys)
{
	unsigned int i4larbid = smiclk_subsys_2_larb(sys);

	if (!fglarbcallback) {
		SMIDBG(1, "don't need backup incb\n");
		return;
	}

	if (i4larbid < SMI_LARB_NR) {
		on_larb_power_off_with_ccf(i4larbid);
#if defined(SMI_D1)
		/* inform m4u to backup register value */
		m4u_larb_restore((int)i4larbid);
#endif
	} else {
		SMIDBG(1, "subsys id don't restore sys %d larb %d\n", sys, i4larbid);
	}

}

struct pg_callbacks smi_clk_subsys_handle = {
	.before_off = smiclk_subsys_before_off,
	.after_on = smiclk_subsys_after_on
};
#endif

static int smi_bwc_config(MTK_SMI_BWC_CONFIG *p_conf, unsigned int *pu4LocalCnt)
{
	int i;
	int result = 0;
	unsigned int u4Concurrency = 0;
	MTK_SMI_BWC_SCEN eFinalScen;
	static MTK_SMI_BWC_SCEN ePreviousFinalScen = SMI_BWC_SCEN_CNT;

	if (smi_tuning_mode == 1) {
		SMIMSG("Doesn't change profile in tunning mode");
		return 0;
	}


	if ((SMI_BWC_SCEN_CNT <= p_conf->scenario) || (0 > p_conf->scenario)) {
		SMIERR("Incorrect SMI BWC config : 0x%x, how could this be...\n", p_conf->scenario);
		return -1;
	}
#if defined(SMI_D1) || defined(SMI_D2) || defined(SMI_D3)
	if (p_conf->b_on_off) {
		/* set mmdvfs step according to certain scenarios */
		mmdvfs_notify_scenario_enter(p_conf->scenario);
	} else {
		/* set mmdvfs step to default after the scenario exits */
		mmdvfs_notify_scenario_exit(p_conf->scenario);
	}
#endif

	spin_lock(&g_SMIInfo.SMI_lock);
	result = fake_mode_handling(p_conf, pu4LocalCnt);
	spin_unlock(&g_SMIInfo.SMI_lock);

	/* Fake mode is not a real SMI profile, so we need to return here */
	if (result == 1)
		return 0;

#if defined(SMI_INTERNAL_CCF_SUPPORT)
	/* prepare larb clk because prepare cannot in spinlock */
	for (i = 0; i < SMI_LARB_NR; i++)
		larb_clock_prepare(i, 1);
#endif
	spin_lock(&g_SMIInfo.SMI_lock);

	if (p_conf->b_on_off) {
		/* turn on certain scenario */
		g_SMIInfo.pu4ConcurrencyTable[p_conf->scenario] += 1;

		if (NULL != pu4LocalCnt)
			pu4LocalCnt[p_conf->scenario] += 1;

	} else {
		/* turn off certain scenario */
		if (0 == g_SMIInfo.pu4ConcurrencyTable[p_conf->scenario]) {
			SMIMSG("Too many turning off for global SMI profile:%d,%d\n",
			       p_conf->scenario, g_SMIInfo.pu4ConcurrencyTable[p_conf->scenario]);
		} else {
			g_SMIInfo.pu4ConcurrencyTable[p_conf->scenario] -= 1;
		}

		if (NULL != pu4LocalCnt) {
			if (0 == pu4LocalCnt[p_conf->scenario]) {
				SMIMSG
				    ("Process : %s did too many turning off for local SMI profile:%d,%d\n",
				     current->comm, p_conf->scenario,
				     pu4LocalCnt[p_conf->scenario]);
			} else {
				pu4LocalCnt[p_conf->scenario] -= 1;
			}
		}
	}

	for (i = 0; i < SMI_BWC_SCEN_CNT; i++) {
		if (g_SMIInfo.pu4ConcurrencyTable[i])
			u4Concurrency |= (1 << i);
	}
#if defined(SMI_D1) || defined(SMI_D2) || defined(SMI_D3)
	/* notify mmdvfs concurrency */
	mmdvfs_notify_scenario_concurrency(u4Concurrency);
#endif

	if ((1 << SMI_BWC_SCEN_MM_GPU) & u4Concurrency)
		eFinalScen = SMI_BWC_SCEN_MM_GPU;
	else if ((1 << SMI_BWC_SCEN_ICFP) & u4Concurrency)
		eFinalScen = SMI_BWC_SCEN_ICFP;
	else if ((1 << SMI_BWC_SCEN_VSS) & u4Concurrency)
		eFinalScen = SMI_BWC_SCEN_VSS;
	else if ((1 << SMI_BWC_SCEN_VR_SLOW) & u4Concurrency)
		eFinalScen = SMI_BWC_SCEN_VR_SLOW;
	else if ((1 << SMI_BWC_SCEN_VR) & u4Concurrency)
		eFinalScen = SMI_BWC_SCEN_VR;
	else if ((1 << SMI_BWC_SCEN_VP) & u4Concurrency)
		eFinalScen = SMI_BWC_SCEN_VP;
	else if ((1 << SMI_BWC_SCEN_SWDEC_VP) & u4Concurrency)
		eFinalScen = SMI_BWC_SCEN_SWDEC_VP;
	else if ((1 << SMI_BWC_SCEN_VENC) & u4Concurrency)
		eFinalScen = SMI_BWC_SCEN_VENC;
	else
		eFinalScen = SMI_BWC_SCEN_NORMAL;

	if (ePreviousFinalScen != eFinalScen) {
		ePreviousFinalScen = eFinalScen;
	} else {
		SMIMSG("Scen equal%d,don't change\n", eFinalScen);
		spin_unlock(&g_SMIInfo.SMI_lock);
#if defined(SMI_INTERNAL_CCF_SUPPORT)
		/* unprepare larb clock */
		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_unprepare(i, 1);
#endif
		return 0;
	}

	/* enable larb clock */
	for (i = 0; i < SMI_LARB_NR; i++)
		larb_clock_enable(i, 1);

	smi_profile = eFinalScen;

	smi_bus_regs_setting(smi_profile,
			smi_profile_config[smi_profile].setting);

	/* Bandwidth Limiter */
	switch (eFinalScen) {
	case SMI_BWC_SCEN_VP:
		SMIMSG("[SMI_PROFILE] : %s\n", "SMI_BWC_VP");
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_VP;
		break;

	case SMI_BWC_SCEN_SWDEC_VP:
		SMIMSG("[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_SWDEC_VP");
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_VP;
		break;

	case SMI_BWC_SCEN_ICFP:
		SMIMSG("[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_ICFP");
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_VR;
		break;
	case SMI_BWC_SCEN_VR:
		SMIMSG("[SMI_PROFILE] : %s\n", "SMI_BWC_VR");
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_VR;
		break;

	case SMI_BWC_SCEN_VR_SLOW:
		SMIMSG("[SMI_PROFILE] : %s\n", "SMI_BWC_VR");
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_NORMAL;
		break;

	case SMI_BWC_SCEN_VENC:
		SMIMSG("[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_VENC");
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_NORMAL;
		break;

	case SMI_BWC_SCEN_NORMAL:
		SMIMSG("[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_NORMAL");
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_NORMAL;
		break;

	case SMI_BWC_SCEN_MM_GPU:
		SMIMSG("[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_MM_GPU");
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_NORMAL;
		break;

	default:
		SMIMSG("[SMI_PROFILE] : %s %d\n", "initSetting", eFinalScen);
		g_smi_bwc_mm_info.hw_ovl_limit = SF_HWC_PIXEL_MAX_NORMAL;
		break;
	}

	/* disable larb clock */
	for (i = 0; i < SMI_LARB_NR; i++)
		larb_clock_disable(i, 1);

	spin_unlock(&g_SMIInfo.SMI_lock);
#if defined(SMI_INTERNAL_CCF_SUPPORT)
	/* unprepare larb clock */
	for (i = 0; i < SMI_LARB_NR; i++)
		larb_clock_unprepare(i, 1);
#endif
	ovl_limit_uevent(smi_profile, g_smi_bwc_mm_info.hw_ovl_limit);

	/* force 30 fps in VR slow motion, because disp driver set fps apis got mutex,
	 * call these APIs only when necessary */
	{
		static unsigned int current_fps;

		if ((eFinalScen == SMI_BWC_SCEN_VR_SLOW) && (current_fps != 30)) {
			/* force 30 fps in VR slow motion profile */
			primary_display_force_set_vsync_fps(30);
			current_fps = 30;
			SMIMSG("[SMI_PROFILE] set 30 fps\n");
		} else if ((eFinalScen != SMI_BWC_SCEN_VR_SLOW) && (current_fps == 30)) {
			/* back to normal fps */
			current_fps = primary_display_get_fps();
			primary_display_force_set_vsync_fps(current_fps);
			SMIMSG("[SMI_PROFILE] back to %u fps\n", current_fps);
		}
	}

	SMIMSG("SMI_PROFILE to:%d %s,cur:%d,%d,%d,%d\n", p_conf->scenario,
	       (p_conf->b_on_off ? "on" : "off"), eFinalScen,
	       g_SMIInfo.pu4ConcurrencyTable[SMI_BWC_SCEN_NORMAL],
	       g_SMIInfo.pu4ConcurrencyTable[SMI_BWC_SCEN_VR],
	       g_SMIInfo.pu4ConcurrencyTable[SMI_BWC_SCEN_VP]);

	return 0;
}

#if !defined(SMI_INTERNAL_CCF_SUPPORT)
struct larb_monitor larb_monitor_handler = {
	.level = LARB_MONITOR_LEVEL_HIGH,
	.backup = on_larb_power_off,
	.restore = on_larb_power_on
};
#endif				/* !defined(SMI_INTERNAL_CCF_SUPPORT) */

int smi_common_init(void)
{
	int i;

#if defined(SMI_INTERNAL_CCF_SUPPORT)
	struct pg_callbacks *pold = 0;
#endif

#if defined(SMI_J)
	/* enable DCM */
	DCM_enable();
#endif
	SMIMSG("Enter smi_common_init\n");
	for (i = 0; i < SMI_LARB_NR; i++) {
		pLarbRegBackUp[i] = kmalloc(LARB_BACKUP_REG_SIZE, GFP_KERNEL | __GFP_ZERO);
		if (pLarbRegBackUp[i] == NULL)
			SMIERR("pLarbRegBackUp kmalloc fail %d\n", i);
	}

	/*
	 * make sure all larb power is on before we register callback func.
	 * then, when larb power is first off, default register value will be backed up.
	 */

	for (i = 0; i < SMI_LARB_NR; i++) {
		larb_clock_prepare(i, 1);
		larb_clock_enable(i, 1);
	}
	/* keep default HW value */
	save_default_common_val(&is_default_value_saved, default_val_smi_l1arb);
	/* set nonconstatnt variables */
	smi_set_nonconstant_variable();
	/* apply init setting after kernel boot */
	SMIMSG("Enter smi_common_init\n");
	smi_bus_regs_setting(SMI_BWC_SCEN_NORMAL, smi_profile_config[SMI_BWC_SCEN_NORMAL].setting);

#if defined(SMI_INTERNAL_CCF_SUPPORT)
	fglarbcallback = true;

	pold = register_pg_callback(&smi_clk_subsys_handle);
	if (pold)
		SMIERR("smi reg clk cb call fail\n");
	else
		SMIMSG("smi reg clk cb call success\n");

#else				/* !defined(SMI_INTERNAL_CCF_SUPPORT) */
	register_larb_monitor(&larb_monitor_handler);
#endif				/* defined(SMI_INTERNAL_CCF_SUPPORT) */

	for (i = 0; i < SMI_LARB_NR; i++) {
		larb_clock_disable(i, 1);
		larb_clock_unprepare(i, 1);
	}

	return 0;
}

static int smi_open(struct inode *inode, struct file *file)
{
	file->private_data = kmalloc_array(SMI_BWC_SCEN_CNT, sizeof(unsigned int), GFP_ATOMIC);

	if (NULL == file->private_data) {
		SMIMSG("Not enough entry for DDP open operation\n");
		return -ENOMEM;
	}

	memset(file->private_data, 0, SMI_BWC_SCEN_CNT * sizeof(unsigned int));

	return 0;
}

static int smi_release(struct inode *inode, struct file *file)
{

#if 0
	unsigned long u4Index = 0;
	unsigned long u4AssignCnt = 0;
	unsigned long *pu4Cnt = (unsigned long *)file->private_data;
	MTK_SMI_BWC_CONFIG config;

	for (; u4Index < SMI_BWC_SCEN_CNT; u4Index += 1) {
		if (pu4Cnt[u4Index]) {
			SMIMSG("Process:%s does not turn off BWC properly , force turn off %d\n",
			       current->comm, u4Index);
			u4AssignCnt = pu4Cnt[u4Index];
			config.b_on_off = 0;
			config.scenario = (MTK_SMI_BWC_SCEN) u4Index;
			do {
				smi_bwc_config(&config, pu4Cnt);
			} while (0 < u4AssignCnt);
		}
	}
#endif

	if (NULL != file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0;
}

static long smi_ioctl(struct file *pFile, unsigned int cmd, unsigned long param)
{
	int ret = 0;

	/* unsigned long * pu4Cnt = (unsigned long *)pFile->private_data; */

	switch (cmd) {

		/* disable reg access ioctl by default for possible security holes */
		/* TBD: check valid SMI register range */
	case MTK_IOC_SMI_BWC_CONFIG:{
			MTK_SMI_BWC_CONFIG cfg;

			ret = copy_from_user(&cfg, (void *)param, sizeof(MTK_SMI_BWC_CONFIG));
			if (ret) {
				SMIMSG(" SMI_BWC_CONFIG, copy_from_user failed: %d\n", ret);
				return -EFAULT;
			}
			ret = smi_bwc_config(&cfg, NULL);

			break;
		}
		/* GMP start */
	case MTK_IOC_SMI_BWC_INFO_SET:{
			smi_set_mm_info_ioctl_wrapper(pFile, cmd, param);
			break;
		}
	case MTK_IOC_SMI_BWC_INFO_GET:{
			smi_get_mm_info_ioctl_wrapper(pFile, cmd, param);
			break;
		}
		/* GMP end */

	case MTK_IOC_SMI_DUMP_LARB:{
			unsigned int larb_index;

			ret = copy_from_user(&larb_index, (void *)param, sizeof(unsigned int));
			if (ret)
				return -EFAULT;

			smi_dumpLarb(larb_index);
		}
		break;

	case MTK_IOC_SMI_DUMP_COMMON:{
			unsigned int arg;

			ret = copy_from_user(&arg, (void *)param, sizeof(unsigned int));
			if (ret)
				return -EFAULT;

			smi_dumpCommon();
		}
		break;
#if defined(SMI_D1) || defined(SMI_D2) || defined(SMI_D3)
	case MTK_IOC_MMDVFS_CMD:
		{
			MTK_MMDVFS_CMD mmdvfs_cmd;

			if (copy_from_user(&mmdvfs_cmd, (void *)param, sizeof(MTK_MMDVFS_CMD)))
				return -EFAULT;


			mmdvfs_handle_cmd(&mmdvfs_cmd);

			if (copy_to_user
			    ((void *)param, (void *)&mmdvfs_cmd, sizeof(MTK_MMDVFS_CMD))) {
				return -EFAULT;
			}
		}
		break;
#endif
	default:
		return -1;
	}

	return ret;
}

static const struct file_operations smiFops = {
	.owner = THIS_MODULE,
	.open = smi_open,
	.release = smi_release,
	.unlocked_ioctl = smi_ioctl,
	.compat_ioctl = MTK_SMI_COMPAT_ioctl,
};

#if defined(SMI_J)
/*
static int smi_sel;

static ssize_t smi_sel_show(struct kobject *kobj, struct kobj_attribute *attr,
char *buf)
{
	char *p = buf;

	p += sprintf(p, "%d\n", smi_sel);

	return p - buf;
}

static ssize_t smi_sel_store(struct kobject *kobj, struct kobj_attribute *attr,
const char *buf, size_t count)
{
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EPERM;

	smi_sel = val;

	return count;
}

static ssize_t smi_dbg_show(struct kobject *kobj, struct kobj_attribute *attr,
char *buf)
{
	if (smi_sel >= 0 && smi_sel < SMI_LARB_NR)
		smi_dumpLarb(smi_sel);
	else if (smi_sel == 999)
		smi_dumpCommon();

	return 0;
}
DEFINE_ATTR_RW(smi_sel);
DEFINE_ATTR_RO(smi_dbg);

static struct attribute *smi_attrs[] = {__ATTR_OF(smi_sel), __ATTR_OF(smi_dbg),
NULL, };

static struct attribute_group smi_attr_group = {.name = "smi", .attrs =
smi_attrs, };
*/
#endif
static dev_t smiDevNo = MKDEV(MTK_SMI_MAJOR_NUMBER, 0);
static inline int smi_register(void)
{
	if (alloc_chrdev_region(&smiDevNo, 0, 1, "MTK_SMI")) {
		SMIERR("Allocate device No. failed");
		return -EAGAIN;
	}
	/* Allocate driver */
	pSmiDev = cdev_alloc();

	if (NULL == pSmiDev) {
		unregister_chrdev_region(smiDevNo, 1);
		SMIERR("Allocate mem for kobject failed");
		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(pSmiDev, &smiFops);
	pSmiDev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(pSmiDev, smiDevNo, 1)) {
		SMIERR("Attatch file operation failed");
		unregister_chrdev_region(smiDevNo, 1);
		return -EAGAIN;
	}

	return 0;
}

static unsigned long get_register_base(int i)
{
	unsigned long pa_value = 0;
	unsigned long va_value = 0;

	va_value = gSMIBaseAddrs[i];
	pa_value = virt_to_phys((void *)va_value);

	return pa_value;
}

void register_base_dump(void)
{
	int i = 0;

	for (i = 0; i < SMI_REG_REGION_MAX; i++) {
		SMIMSG("REG BASE:%s-->VA=0x%lx,PA=0x%lx\n",
		       smi_get_region_name(i), gSMIBaseAddrs[i], get_register_base(i));
	}
}

static struct class *pSmiClass;

static int smi_probe(struct platform_device *pdev)
{

	int i;

	static unsigned int smi_probe_cnt;
	struct device *smiDevice = NULL;

	SMIMSG("Enter smi_probe\n");
	/* Debug only */
	if (smi_probe_cnt != 0) {
		SMIERR("Only support 1 SMI driver probed\n");
		return 0;
	}
	smi_probe_cnt++;
	SMIMSG("Allocate smi_dev space\n");
	smi_dev = kmalloc(sizeof(struct smi_device), GFP_KERNEL);

	if (smi_dev == NULL) {
		SMIERR("Unable to allocate memory for smi driver\n");
		return -ENOMEM;
	}
	if (NULL == pdev) {
		SMIERR("platform data missed\n");
		return -ENXIO;
	}
	/* Keep the device structure */
	smi_dev->dev = &pdev->dev;

	/* Map registers */
	for (i = 0; i < SMI_REG_REGION_MAX; i++) {
		SMIMSG("Save region: %d\n", i);
		smi_dev->regs[i] = (void *)of_iomap(pdev->dev.of_node, i);

		if (!smi_dev->regs[i]) {
			SMIERR("Unable to ioremap registers, of_iomap fail, i=%d\n", i);
			return -ENOMEM;
		}
		/* Record the register base in global variable */
		gSMIBaseAddrs[i] = (unsigned long)(smi_dev->regs[i]);
		SMIMSG("DT, i=%d, region=%s, map_addr=0x%p, reg_pa=0x%lx\n", i,
		       smi_get_region_name(i), smi_dev->regs[i], get_register_base(i));
	}

#if defined(SMI_INTERNAL_CCF_SUPPORT)
	smi_dev->smi_common_clk = get_smi_clk("smi-common");
	smi_dev->smi_larb0_clk = get_smi_clk("smi-larb0");
	smi_dev->img_larb2_clk = get_smi_clk("img-larb2");
#if defined(SMI_D1)
	smi_dev->vdec0_vdec_clk = get_smi_clk("vdec0-vdec");
#endif
	smi_dev->vdec1_larb_clk = get_smi_clk("vdec1-larb");
	smi_dev->venc_larb_clk = get_smi_clk("venc-larb");
#if defined(SMI_J)
	smi_dev->venc_venc_clk = get_smi_clk("venc-venc");
#endif
	/* MTCMOS */
	smi_dev->larb1_mtcmos = get_smi_clk("mtcmos-vde");
	smi_dev->larb3_mtcmos = get_smi_clk("mtcmos-ven");
	smi_dev->larb2_mtcmos = get_smi_clk("mtcmos-isp");
	smi_dev->larb0_mtcmos = get_smi_clk("mtcmos-dis");
#endif

	SMIMSG("Execute smi_register\n");
	if (smi_register()) {
		dev_err(&pdev->dev, "register char failed\n");
		return -EAGAIN;
	}

	pSmiClass = class_create(THIS_MODULE, "MTK_SMI");
	if (IS_ERR(pSmiClass)) {
		int ret = PTR_ERR(pSmiClass);

		SMIERR("Unable to create class, err = %d", ret);
		return ret;
	}
	SMIMSG("Create davice\n");
	smiDevice = device_create(pSmiClass, NULL, smiDevNo, NULL, "MTK_SMI");
	smiDeviceUevent = smiDevice;

	SMIMSG("SMI probe done.\n");
#if defined(SMI_D2)
	/* To adapt the legacy codes */
	smi_reg_base_common_ext = gSMIBaseAddrs[SMI_COMMON_REG_INDX];
	smi_reg_base_barb0 = gSMIBaseAddrs[SMI_LARB0_REG_INDX];
	smi_reg_base_barb1 = gSMIBaseAddrs[SMI_LARB1_REG_INDX];
	smi_reg_base_barb2 = gSMIBaseAddrs[SMI_LARB2_REG_INDX];
	/* smi_reg_base_barb4 = gSMIBaseAddrs[SMI_LARB4_REG_INDX]; */

	gLarbBaseAddr[0] = LARB0_BASE;
	gLarbBaseAddr[1] = LARB1_BASE;
	gLarbBaseAddr[2] = LARB2_BASE;
#elif defined(SMI_D1) || defined(SMI_D3)
	/* To adapt the legacy codes */
	smi_reg_base_common_ext = gSMIBaseAddrs[SMI_COMMON_REG_INDX];
	smi_reg_base_barb0 = gSMIBaseAddrs[SMI_LARB0_REG_INDX];
	smi_reg_base_barb1 = gSMIBaseAddrs[SMI_LARB1_REG_INDX];
	smi_reg_base_barb2 = gSMIBaseAddrs[SMI_LARB2_REG_INDX];
	smi_reg_base_barb3 = gSMIBaseAddrs[SMI_LARB3_REG_INDX];

	gLarbBaseAddr[0] = LARB0_BASE;
	gLarbBaseAddr[1] = LARB1_BASE;
	gLarbBaseAddr[2] = LARB2_BASE;
	gLarbBaseAddr[3] = LARB3_BASE;

#elif defined(SMI_J)
/* To adapt the legacy codes */
	smi_reg_base_common_ext = gSMIBaseAddrs[SMI_COMMON_REG_INDX];
	smi_reg_base_barb0 = gSMIBaseAddrs[SMI_LARB0_REG_INDX];
	smi_reg_base_barb1 = gSMIBaseAddrs[SMI_LARB1_REG_INDX];
	smi_reg_base_barb2 = gSMIBaseAddrs[SMI_LARB2_REG_INDX];
	smi_reg_base_barb3 = gSMIBaseAddrs[SMI_LARB3_REG_INDX];

	gLarbBaseAddr[0] = LARB0_BASE;
	gLarbBaseAddr[1] = LARB1_BASE;
	gLarbBaseAddr[2] = LARB2_BASE;
	gLarbBaseAddr[3] = LARB3_BASE;
#else
	smi_reg_base_common_ext = gSMIBaseAddrs[SMI_COMMON_REG_INDX];
	smi_reg_base_barb0 = gSMIBaseAddrs[SMI_LARB0_REG_INDX];
	smi_reg_base_barb1 = gSMIBaseAddrs[SMI_LARB1_REG_INDX];

	gLarbBaseAddr[0] = LARB0_BASE;
	gLarbBaseAddr[1] = LARB1_BASE;
#endif

	SMIMSG("Execute smi_common_init\n");
	smi_common_init();

	return 0;

}

char *smi_get_region_name(unsigned int region_indx)
{
	switch (region_indx) {
	case SMI_COMMON_REG_INDX:
		return "smi_common";
	case SMI_LARB0_REG_INDX:
		return "larb0";
	case SMI_LARB1_REG_INDX:
		return "larb1";
	case SMI_LARB2_REG_INDX:
		return "larb2";
	case SMI_LARB3_REG_INDX:
		return "larb3";
	default:
		SMIMSG("invalid region id=%d", region_indx);
		return "unknown";
	}
}

static int smi_remove(struct platform_device *pdev)
{
	cdev_del(pSmiDev);
	unregister_chrdev_region(smiDevNo, 1);
	device_destroy(pSmiClass, smiDevNo);
	class_destroy(pSmiClass);
	return 0;
}

static int smi_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int smi_resume(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id smi_of_ids[] = {
	{.compatible = "mediatek,smi_common",},
	{}
};

static struct platform_driver smiDrv = {
	.probe = smi_probe,
	.remove = smi_remove,
	.suspend = smi_suspend,
	.resume = smi_resume,
	.driver = {
		   .name = "MTK_SMI",
		   .owner = THIS_MODULE,
		   .of_match_table = smi_of_ids,
		   }
};

static int __init smi_init(void)
{
	SMIMSG("smi_init enter\n");
	spin_lock_init(&g_SMIInfo.SMI_lock);
#if defined(SMI_D1) || defined(SMI_D2) || defined(SMI_D3)
	/* MMDVFS init */
	mmdvfs_init(&g_smi_bwc_mm_info);
#endif
	memset(g_SMIInfo.pu4ConcurrencyTable, 0, SMI_BWC_SCEN_CNT * sizeof(unsigned int));

	/* Informs the kernel about the function to be called */
	/* if hardware matching MTK_SMI has been found */
	SMIMSG("register platform driver\n");
	if (platform_driver_register(&smiDrv)) {
		SMIERR("failed to register MAU driver");
		return -ENODEV;
	}
	SMIMSG("exit smi_init\n");
	return 0;
}

static void __exit smi_exit(void)
{
	platform_driver_unregister(&smiDrv);

}







void smi_client_status_change_notify(int module, int mode)
{

}

#if defined(SMI_J)
MTK_SMI_BWC_SCEN smi_get_current_profile(void)
{
	return (MTK_SMI_BWC_SCEN) smi_profile;
}
#endif
#if IS_ENABLED(CONFIG_COMPAT)
/* 32 bits process ioctl support: */
/* This is prepared for the future extension since currently the sizes of 32 bits */
/* and 64 bits smi parameters are the same. */

struct MTK_SMI_COMPAT_BWC_CONFIG {
	compat_int_t scenario;
	compat_int_t b_on_off;	/* 0 : exit this scenario , 1 : enter this scenario */
};

struct MTK_SMI_COMPAT_BWC_INFO_SET {
	compat_int_t property;
	compat_int_t value1;
	compat_int_t value2;
};

struct MTK_SMI_COMPAT_BWC_MM_INFO {
	compat_uint_t flag;	/* Reserved */
	compat_int_t concurrent_profile;
	compat_int_t sensor_size[2];
	compat_int_t video_record_size[2];
	compat_int_t display_size[2];
	compat_int_t tv_out_size[2];
	compat_int_t fps;
	compat_int_t video_encode_codec;
	compat_int_t video_decode_codec;
	compat_int_t hw_ovl_limit;
};

#define COMPAT_MTK_IOC_SMI_BWC_CONFIG      MTK_IOW(24, struct MTK_SMI_COMPAT_BWC_CONFIG)
#define COMPAT_MTK_IOC_SMI_BWC_INFO_SET    MTK_IOWR(28, struct MTK_SMI_COMPAT_BWC_INFO_SET)
#define COMPAT_MTK_IOC_SMI_BWC_INFO_GET    MTK_IOWR(29, struct MTK_SMI_COMPAT_BWC_MM_INFO)

static int compat_get_smi_bwc_config_struct(struct MTK_SMI_COMPAT_BWC_CONFIG __user *data32,
					    MTK_SMI_BWC_CONFIG __user *data)
{

	compat_int_t i;
	int err;

	/* since the int sizes of 32 A32 and A64 are equal so we don't convert them actually here */
	err = get_user(i, &(data32->scenario));
	err |= put_user(i, &(data->scenario));
	err |= get_user(i, &(data32->b_on_off));
	err |= put_user(i, &(data->b_on_off));

	return err;
}

static int compat_get_smi_bwc_mm_info_set_struct(struct MTK_SMI_COMPAT_BWC_INFO_SET __user *data32,
						 MTK_SMI_BWC_INFO_SET __user *data)
{

	compat_int_t i;
	int err;

	/* since the int sizes of 32 A32 and A64 are equal so we don't convert them actually here */
	err = get_user(i, &(data32->property));
	err |= put_user(i, &(data->property));
	err |= get_user(i, &(data32->value1));
	err |= put_user(i, &(data->value1));
	err |= get_user(i, &(data32->value2));
	err |= put_user(i, &(data->value2));

	return err;
}

static int compat_get_smi_bwc_mm_info_struct(struct MTK_SMI_COMPAT_BWC_MM_INFO __user *data32,
					     MTK_SMI_BWC_MM_INFO __user *data)
{
	compat_uint_t u;
	compat_int_t i;
	compat_int_t p[2];
	int err;

	/* since the int sizes of 32 A32 and A64 are equal so we don't convert them actually here */
	err = get_user(u, &(data32->flag));
	err |= put_user(u, &(data->flag));
	err |= get_user(i, &(data32->concurrent_profile));
	err |= put_user(i, &(data->concurrent_profile));
	err |= copy_from_user(p, &(data32->sensor_size), sizeof(p));
	err |= copy_to_user(&(data->sensor_size), p, sizeof(p));
	err |= copy_from_user(p, &(data32->video_record_size), sizeof(p));
	err |= copy_to_user(&(data->video_record_size), p, sizeof(p));
	err |= copy_from_user(p, &(data32->display_size), sizeof(p));
	err |= copy_to_user(&(data->display_size), p, sizeof(p));
	err |= copy_from_user(p, &(data32->tv_out_size), sizeof(p));
	err |= copy_to_user(&(data->tv_out_size), p, sizeof(p));
	err |= get_user(i, &(data32->fps));
	err |= put_user(i, &(data->fps));
	err |= get_user(i, &(data32->video_encode_codec));
	err |= put_user(i, &(data->video_encode_codec));
	err |= get_user(i, &(data32->video_decode_codec));
	err |= put_user(i, &(data->video_decode_codec));
	err |= get_user(i, &(data32->hw_ovl_limit));
	err |= put_user(i, &(data->hw_ovl_limit));

	return err;
}

static int compat_put_smi_bwc_mm_info_struct(struct MTK_SMI_COMPAT_BWC_MM_INFO __user *data32,
					     MTK_SMI_BWC_MM_INFO __user *data)
{

	compat_uint_t u;
	compat_int_t i;
	compat_int_t p[2];
	int err;

	/* since the int sizes of 32 A32 and A64 are equal so we don't convert them actually here */
	err = get_user(u, &(data->flag));
	err |= put_user(u, &(data32->flag));
	err |= get_user(i, &(data->concurrent_profile));
	err |= put_user(i, &(data32->concurrent_profile));
	err |= copy_from_user(p, &(data->sensor_size), sizeof(p));
	err |= copy_to_user(&(data32->sensor_size), p, sizeof(p));
	err |= copy_from_user(p, &(data->video_record_size), sizeof(p));
	err |= copy_to_user(&(data32->video_record_size), p, sizeof(p));
	err |= copy_from_user(p, &(data->display_size), sizeof(p));
	err |= copy_to_user(&(data32->display_size), p, sizeof(p));
	err |= copy_from_user(p, &(data->tv_out_size), sizeof(p));
	err |= copy_to_user(&(data32->tv_out_size), p, sizeof(p));
	err |= get_user(i, &(data->fps));
	err |= put_user(i, &(data32->fps));
	err |= get_user(i, &(data->video_encode_codec));
	err |= put_user(i, &(data32->video_encode_codec));
	err |= get_user(i, &(data->video_decode_codec));
	err |= put_user(i, &(data32->video_decode_codec));
	err |= get_user(i, &(data->hw_ovl_limit));
	err |= put_user(i, &(data32->hw_ovl_limit));
	return err;
}

static long MTK_SMI_COMPAT_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_MTK_IOC_SMI_BWC_CONFIG:
		{
			if (COMPAT_MTK_IOC_SMI_BWC_CONFIG == MTK_IOC_SMI_BWC_CONFIG) {
				return filp->f_op->unlocked_ioctl(filp, cmd,
								  (unsigned long)compat_ptr(arg));
			} else {

				struct MTK_SMI_COMPAT_BWC_CONFIG __user *data32;
				MTK_SMI_BWC_CONFIG __user *data;
				int err;

				data32 = compat_ptr(arg);
				data = compat_alloc_user_space(sizeof(MTK_SMI_BWC_CONFIG));

				if (data == NULL)
					return -EFAULT;

				err = compat_get_smi_bwc_config_struct(data32, data);
				if (err)
					return err;

				ret = filp->f_op->unlocked_ioctl(filp, MTK_IOC_SMI_BWC_CONFIG,
								 (unsigned long)data);
				return ret;
			}
		}

	case COMPAT_MTK_IOC_SMI_BWC_INFO_SET:
		{

			if (COMPAT_MTK_IOC_SMI_BWC_INFO_SET == MTK_IOC_SMI_BWC_INFO_SET) {
				return filp->f_op->unlocked_ioctl(filp, cmd,
								  (unsigned long)compat_ptr(arg));
			} else {

				struct MTK_SMI_COMPAT_BWC_INFO_SET __user *data32;
				MTK_SMI_BWC_INFO_SET __user *data;
				int err;

				data32 = compat_ptr(arg);
				data = compat_alloc_user_space(sizeof(MTK_SMI_BWC_INFO_SET));
				if (data == NULL)
					return -EFAULT;

				err = compat_get_smi_bwc_mm_info_set_struct(data32, data);
				if (err)
					return err;

				return filp->f_op->unlocked_ioctl(filp, MTK_IOC_SMI_BWC_INFO_SET,
								  (unsigned long)data);
			}
		}
		/* Fall through */
	case COMPAT_MTK_IOC_SMI_BWC_INFO_GET:
		{

			if (COMPAT_MTK_IOC_SMI_BWC_INFO_GET == MTK_IOC_SMI_BWC_INFO_GET) {
				return filp->f_op->unlocked_ioctl(filp, cmd,
								  (unsigned long)compat_ptr(arg));
			} else {
				struct MTK_SMI_COMPAT_BWC_MM_INFO __user *data32;
				MTK_SMI_BWC_MM_INFO __user *data;
				int err;

				data32 = compat_ptr(arg);
				data = compat_alloc_user_space(sizeof(MTK_SMI_BWC_MM_INFO));

				if (data == NULL)
					return -EFAULT;

				err = compat_get_smi_bwc_mm_info_struct(data32, data);
				if (err)
					return err;

				ret = filp->f_op->unlocked_ioctl(filp, MTK_IOC_SMI_BWC_INFO_GET,
								 (unsigned long)data);

				err = compat_put_smi_bwc_mm_info_struct(data32, data);

				if (err)
					return err;

				return ret;
			}
		}

	case MTK_IOC_SMI_DUMP_LARB:
	case MTK_IOC_SMI_DUMP_COMMON:
	case MTK_IOC_MMDVFS_CMD:

		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
	default:
		return -ENOIOCTLCMD;
	}

}

#endif

#if defined(SMI_J)
int is_mmdvfs_freq_hopping_disabled(void)
{
	return disable_freq_hopping;
}

int is_mmdvfs_freq_mux_disabled(void)
{
	return disable_freq_mux;
}

int is_force_max_mmsys_clk(void)
{
	return force_max_mmsys_clk;
}

int is_force_camera_hpm(void)
{
	return force_camera_hpm;
}
subsys_initcall(smi_init);
module_param_named(disable_freq_hopping, disable_freq_hopping, uint, S_IRUGO | S_IWUSR);
module_param_named(disable_freq_mux, disable_freq_mux, uint, S_IRUGO | S_IWUSR);
module_param_named(force_max_mmsys_clk, force_max_mmsys_clk, uint, S_IRUGO | S_IWUSR);
module_param_named(force_camera_hpm, force_camera_hpm, uint, S_IRUGO | S_IWUSR);
#endif
module_init(smi_init);
module_exit(smi_exit);

module_param_named(debug_level, smi_debug_level, uint, S_IRUGO | S_IWUSR);
module_param_named(tuning_mode, smi_tuning_mode, uint, S_IRUGO | S_IWUSR);
module_param_named(wifi_disp_transaction, wifi_disp_transaction, uint, S_IRUGO | S_IWUSR);

MODULE_DESCRIPTION("MTK SMI driver");
MODULE_AUTHOR("Kendrick Hsu<kendrick.hsu@mediatek.com>");
MODULE_LICENSE("GPL");
