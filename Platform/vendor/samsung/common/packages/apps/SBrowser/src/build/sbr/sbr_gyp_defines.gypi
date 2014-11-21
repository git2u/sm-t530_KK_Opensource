
{
    'variables': {
        'conditions': [
            ['OS=="android"', {
                'android_src%':1,
                'android_product_out%': 0,
                'enable_sbrowser%': 1,
                'enable_sbrowser_devicemotion%': 1,
                'enable_sbrowser_fingerprint%': 1,
                ########## kikin Modifications Starts ##########
                'enable_sbrowser_kikin%': 1,
                ########### kikin Modifications Ends ###########
                'enable_sbrowser_clipboardex%':1,
                'enable_sbrowser_bookmark%': 1,
                #'enable_sbr_content_view_core%': 1,
                ########## WML Support Modifications Starts ##########
                'enable_sbrowser_wml%': 1,
                ########## WML Support Modifications Starts ##########
                'enable_ui_composition_removal%': 1,
                'enable_sbrowser_apk%' :0,
                'enable_hide_urlbar%': 0,
                'enable_edge_gloweffect%': 1,
                'enable_sbrowser_native_tex_upload%': 1,
                'enable_sbrowser_qcom_skia%': 1,
                'enable_sbrowser_qc_vsync_patch%': 0,
                'enable_sbrowser_batching%': 0, # disabled by crash issues
                'enable_session_cache%': 0,
                ######### vzw Bing search requirement Starts #########
                'enable_sbrowser_bing_search_engine_set_via_javascript': 1,
                ######### vzw Bing search requirement #########		
                ########## ENABLE_SBROWSER_QCOMSO ##########
                'enable_qcom_v8%': 0,
                'enable_cac_feature%': 1,
                ########## ENABLE_SBROWSER_SECSO ##########
                'enable_sec_v8%': 0,
                ########## Enable webrtc feature: syam.dsp@samsung.com ##########
                'enable_sbrowser_webrtc%': 1,
		########## Enable CSC feature ##########
		'enable_csc_feature%': 1,
		'enable_angle_memory_feature%': 1,
		'disable_robustness_extension%': 1,
		########## Backported Features ##########
		'disable_composited_antialiasing%': 0,
		'enable_update_tile_pririty_transform_patch%':1,		
		'enable_platform_openssl%': 1,
                'enable_platform_zlib%': 0,
		###### composing region drawing feature for JPN #######
		'enable_jpn_composing_region_feature%': 0,
		'enable_so_reduction%': 0,
		'enable_svg%': 1,
                ############ enable revocation check of certificate ############ 
                'enable_revocation_check%': 1,
                ############ enable cookie quick cookie flash to db ############ 
                'enable_cookie_quick_flash%': 0,
		############ enable sbr adblocker ############
                'enable_sbr_adblocker%': 0,
                ############ opera turbo solution ############ 
                'enable_data_compression%': 0,
            }],
        ],
    },
    ######### Location of Android directories #########
    'android_src%': '<!(/bin/echo -n $SBR_ANDROID_SRC_ROOT)',
    'android_product_out%': '<!(/bin/echo -n $ANDROID_PRODUCT_OUT)',
    'enable_sbrowser%': '<(enable_sbrowser)',
    'enable_sbrowser_devicemotion%': '<(enable_sbrowser_devicemotion)',
    'enable_sbrowser_fingerprint%': '<(enable_sbrowser_fingerprint)',
    ################ kikin Modification Starts ################
    'enable_sbrowser_kikin%': '<(enable_sbrowser_kikin)',
    ################ kikin Modification Ends ##################
    'enable_sbrowser_clipboardex%':'<(enable_sbrowser_clipboardex)',
    'enable_sbrowser_bookmark%': '<(enable_sbrowser_bookmark)',
    #'enable_sbr_content_view_core%': '<(enable_sbr_content_view_core)',
    ########## WML Support Modifications Starts ##########
    'enable_sbrowser_wml%': '<(enable_sbrowser_wml)',
    ########## WML Support Modifications Starts ##########
    'enable_ui_composition_removal%': '<(enable_ui_composition_removal)',
    'enable_sbrowser_apk%': '<(enable_sbrowser_apk)',
    'enable_hide_urlbar%': '<(enable_hide_urlbar)',
    'enable_edge_gloweffect%': '<(enable_edge_gloweffect)',
    'enable_sbrowser_native_tex_upload%': '<(enable_sbrowser_native_tex_upload)',
    'enable_sbrowser_qc_vsync_patch%': '<(enable_sbrowser_qc_vsync_patch)',
    'enable_sbrowser_batching%': '<(enable_sbrowser_batching)',	
    'enable_session_cache%': '<(enable_session_cache)',   
    ######### vzw Bing search requirement Starts #########
    'enable_sbrowser_bing_search_engine_set_via_javascript%': '<(enable_sbrowser_bing_search_engine_set_via_javascript)',
    ######### vzw Bing search requirement ######### 
    ########## ENABLE_SBROWSER_QCOMSO ##########
    'enable_qcom_v8%': '<(enable_qcom_v8)',
    'enable_sbrowser_qcom_skia%': '<(enable_sbrowser_qcom_skia)',
    'enable_cac_feature%' : '<(enable_cac_feature)',
    ########## ENABLE_SBROWSER_SECSO ##########
    'enable_sec_v8%': '<(enable_sec_v8)',
    ########## Enable webrtc feature: syam.dsp@samsung.com ##########
    'enable_sbrowser_webrtc%': '<(enable_sbrowser_webrtc)',
    ########## Enable CSC feature ##########
    'enable_csc_feature%' : '<(enable_csc_feature)',
    'enable_angle_memory_feature%': '<(enable_angle_memory_feature)',
    'disable_robustness_extension%': '<(disable_robustness_extension)',
    ########## Backported Features ##########    
    'disable_composited_antialiasing%': '<(disable_composited_antialiasing)',
    'enable_update_tile_pririty_transform_patch%': '<(enable_update_tile_pririty_transform_patch)',    
   'enable_platform_openssl%': '<(enable_platform_openssl)',
    'enable_platform_zlib%': '<(enable_platform_zlib)',
    ###### composing region drawing feature for JPN #######
    'enable_jpn_composing_region_feature%' :'<!(/bin/echo -n $SBROWSER_ENABLE_SUB_COMPOSING)',
    'enable_so_reduction%' : '<(enable_so_reduction)',
    'enable_svg%' : '<(enable_svg)',
    #### enable revocation check of certificate ####
    'enable_revocation_check%': '<(enable_revocation_check)',
    'enable_cookie_quick_flash%': '<(enable_cookie_quick_flash)',
    'enable_sbr_adblocker%': '<(enable_sbr_adblocker)',
    ############ opera turbo solution ############
    'enable_data_compression%': '<(enable_data_compression)',
}
