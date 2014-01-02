static int msm_fb_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct msm_fb_panel_data *pdata;
	struct fb_info *fbi;
	int rc;
	int err = 0;

	MSM_FB_DEBUG("msm_fb_probe\n");

	if ((pdev->id == 0) && (pdev->num_resources > 0)) {
		msm_fb_pdata = pdev->dev.platform_data;
		fbram_size = pdev->resource[0].end - pdev->resource[0].start + 1;
		fbram_phys = (char *)pdev->resource[0].start;
		fbram = ioremap((unsigned long)fbram_phys, fbram_size);

		if (!fbram) {
			printk(KERN_ERR "fbram ioremap failed!\n");
			return -ENOMEM;
		}
		MSM_FB_INFO("msm_fb_probe:  phy_Addr = 0x%x virt = 0x%x\n",
			     (int)fbram_phys, (int)fbram);

		msm_fb_resource_initialized = 1;
#ifdef CONFIG_FB_MSM_OVERLAY
		/* TODO: find a better way to pass blt_base to overlay */
		if (pdev->num_resources > 1) {
			ov_blt_base = pdev->resource[1].start;
			ov_blt_size = pdev->resource[1].end -
				pdev->resource[1].start + 1;
		}
#endif
		return 0;
	}

	if (!msm_fb_resource_initialized || msm_fb_pdata == NULL)
		return -EPERM;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (pdev_list_cnt >= MSM_FB_MAX_DEV_LIST)
		return -ENOMEM;

	mfd->panel_info.frame_count = 0;
	mfd->width = msm_fb_pdata->width;
	mfd->height = msm_fb_pdata->height;
#ifdef CONFIG_LCDC_TURN_ON_BL_AFTER_PANEL_ON
	mfd->bl_level = MAX_BACKLIGHT_BRIGHTNESS;
#else
	mfd->bl_level = 0;
#endif
#ifdef CONFIG_FB_MSM_OVERLAY
	mfd->overlay_play_enable = 1;

	/* TODO: find a better way to pass blt_base to overlay */
	err = device_create_file(&pdev->dev, &dev_attr_fbdebug);
	if (err != 0)
		printk(KERN_WARNING "attr_fbdebug failed\n");
	mfd->blt_base = ov_blt_base;
	mfd->blt_size = ov_blt_size;
	if (msm_fb_pdata && mfd->panel_info.type == MIPI_CMD_PANEL) {
		mfd->blt_mode = msm_fb_pdata->blt_mode;
		mfd->panel_info.is_3d_panel = msm_fb_pdata->is_3d_panel;
		PR_DISP_INFO("%s: blt_mode=%d is_3d_pane=%d\n", __func__, mfd->blt_mode, msm_fb_pdata->is_3d_panel);
	}
#endif

	rc = msm_fb_register(mfd);
	if (rc)
		return rc;
	err = pm_runtime_set_active(mfd->fbi->dev);
	if (err < 0)
		printk(KERN_ERR "pm_runtime: fail to set active.\n");
	pm_runtime_enable(mfd->fbi->dev);
#ifdef CONFIG_FB_BACKLIGHT
	msm_fb_config_backlight(mfd);
#else
	/* android supports only one lcd-backlight/lcd for now */
	if (!lcd_backlight_registered) {
		if (led_classdev_register(&pdev->dev, &backlight_led))
			printk(KERN_ERR "led_classdev_register failed\n");
		else {
			lcd_backlight_registered = 1;
			if (device_create_file(backlight_led.dev, &auto_attr))
				PR_DISP_INFO("attr creation failed\n");
		}
	}
#endif
	pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;
#if defined CONFIG_FB_MSM_SELF_REFRESH
	if ((pdata) && (pdata->self_refresh_switch)) {
		INIT_WORK(&mfd->self_refresh_work, self_refresh_do_work);
		mfd->self_refresh_wq = create_workqueue("self_refresh_wq");
		if (!mfd->self_refresh_wq)
			PR_DISP_ERR("%s: can't create workqueue\n", __func__);
		setup_timer(&mfd->self_refresh_timer, self_refresh_update, (unsigned long)mfd);
	}
#endif
	pdev_list[pdev_list_cnt++] = pdev;
	msm_fb_create_sysfs(pdev);
	if (mfd->panel_info.type != DTV_PANEL &&
		mfd->panel_info.type != TV_PANEL) {
		fbi = mfd->fbi;
		if (msm_fb_blank_sub(FB_BLANK_UNBLANK, fbi, mfd->op_enable)) {
			printk(KERN_ERR "msm_fb_open: can't turn on display!\n");
			return -1;
		}
	}
#if defined (CONFIG_FB_MSM_MDP_ABL)
	mfd->enable_abl = false;
#endif
	return 0;
}
