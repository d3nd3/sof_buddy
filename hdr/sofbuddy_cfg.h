#ifndef SOFBUDDY_CFG_H
#define SOFBUDDY_CFG_H

void sofbuddy_cfg_exec_startup(void);
void sofbuddy_cfg_save_now(void);
void sofbuddy_cfg_cl_shutdown_post_callback(void);
void sofbuddy_cfg_save_sofplus_settings(void);
void sofbuddy_cfg_watch_spm_cvars(void);
void sofbuddy_cfg_spm_save_disable(void);
void sofbuddy_cfg_spm_save_enable(void);
void sofbuddy_cfg_qcommon_frame_post_callback(int msec);

#endif // SOFBUDDY_CFG_H
