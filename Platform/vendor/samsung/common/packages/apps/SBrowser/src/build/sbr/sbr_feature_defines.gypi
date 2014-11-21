
{
'conditions': [
    ['enable_sbrowser==1', {
     'includes': ['sbr_bugfixes_defines.gypi'],
          'defines':[
		     'SBROWSER_SUPPORT=1',                 #Flag should not be used for open source bug fix and feature implementation
	            ],
		   }],
    ['enable_sbrowser_apk==1', {
	'defines':[
        'SBROWSER_DEVICEMOTION_IMPL=1',       #Flag used to enable device motion feature
#        'SBROWSER_HISTORY_CONTROLLER=1',
        'SBROWSER_CHROME_NATIVE_PREFERENCES=1',
        'SBROWSER_IMAGEDRAG_IMPL=1',          #Flag used to provide the Image data to APP side on long press
        'SBROWSER_TEMP_UASTRING=1',           #Flag used to hard code the user agent string
#        'SBROWSER_COMMIT_BLOCK_ONTOUCH=1',	   #For touch performace improvement
#        'SBROWSER_PAUSE_ANIMATION_ON_SCROLL_ZOOM=1',	   # For touch performace improvement  while zoom/scroll
        'SBROWSER_HISTORY=1',                 #Flag for History changes
        'SBROWSER_HTML5_WEBNOTIFICATION=1',   #Flag For WebNotification
        'SBROWSER_SAVEPAGE_IMPL=1',              #Flag used to enable save page feature
        'SBROWSER_SBRNATIVE_IMPL=1',          #Flag used for sbrcontentview
        'SBROWSER_USE_WIDE_VIEWPORT=1',       #Flag used to enable wide viewport feature
        'SBROWSER_UA_STRING=1',               #Flag used to debug with different UAStrings
        'SBROWSER_PRINT_IMPL=1',		   #Flag used for Print
        'SBROWSER_THUMBNAIL=1',               #Flag used for thumbnail
        'SBROWSER_SOFTBITMAP_IMPL=1',         #Flag used for enabling softbitmap
        'SBROWSER_TEXT_SELECTION=1',          #Flag used for enabling SAMSUNG textselection changes
        'SBROWSER_LINKIFY_TEXT_SELECTION=1', #Flag to enable text selection for plain text linkifiable elements
        'SBROWSER_FORM_NAVIGATION=1',         #Flag used for enabling form navigation
#        'SBROWSER_CONTEXT_MENU=1',            #Flag used for enabling context menu flow
        'SBROWSER_CONTEXT_MENU_FLAG=1',			#Flag used for enabling context menu flow
        'SBROWSER_SCROLLBAR_ANIMATION_OPTIMIZATION=1',            #Flag used to enable all scrollbar specific changes - opaque scrollbar, block scrollbar animation to reduce power consumption
        'SBROWSER_BLOCK_FLINGING_ON_EDGE=1',            #Flag used to block fling on edge position.
        'SBROWSER_FLING_PERFORMANCE_IMPROVEMENT=1',            #Flag used to improve FPS for fling.
        'SBROWSER_DEFER_COMMITS_ON_GESTURE=1', # Flag for deffering commit during gesture action
#        'SBROWSER_SKIP_MODIFYING_TILE_PRIORITY=1', # Flag used to skip modifying tile priority during scroll
        'SBROWSER_BLOCK_SENDING_FRAME_MESSAGE=1', # Flag used not to send compositor frame message during scroll/zoom
        'SBROWSER_DEFAULT_MAX_FRAMES_PENDING=1', # Flag used to increase a pendding frame capability
        'SBROWSER_GENERATING_FAKE_SCROLL_EVENT=1', # Flag used to generate a fake scroll at the beginning of fling
        'SBROWSER_START_FLING_AT_FIRST_VSYNC=1', # Flag used to start the fling animation at the first vsync
#        'SBROWSER_SKIP_GESTURE_EVENT_FILTER=1', # Flag used to skip the gesture event filter concept during scroll/zoom
        'SBROWSER_IMPROVE_FLING_STUTTERING=1', # Flag used to improve stuttering behavior during fling
        'SBROWSER_INFOBAR=1',#Flag for all Samsung type infobar
        'SBROWSER_WML=1',
        'SBROWSER_SBR_NATIVE_CONTENTVIEW=1'
        'SBROWSER_CONTEXT_MENU_FLAG=1',
        'SBROWSER_BOOKMARKS=1',
        'SBROWSER_QUICK_LAUNCH=1',
        'SBROWSER_OPERATOR_BOOKMARKS=1',             #Flag for all operator bookmarks related changes
        'SBROWSER_DOUBLETAP_PATENT_CHANGE=1', #Flag used for double tap block zoom changes
        'SBROWSER_RESIZE_OPTIMIZATION=1', #Flag used for Resize Optimization changes
        'SBROWSER_OVERVIEW_MODE = 1',					#Flag used to enable/disable Overview mode feature
        'SBROWSER_FONT_SUPPORT=1',            #Flag used for Font/Script related changes
        'SBROWSER_FONT_BOOSTING_PREFERENCES=1',         #Flag for Font Boosting menu
        'SBROWSER_EMOJI_FONT_SUPPORT=1',                #Flag for SBrowser Emoji Font Support
        'SBROWSER_HTML_DRAG_N_DROP=1',         #Flag for HTML Drag & Drop feature
        'SBROWSER_SMART_CLIP=1',          #Flag used for enabling SAMSUNG Smart Clip support in SBrowser
        'SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTINSTANT=1',  #Flag for Multi Instant Ui composition removal
        'SBROWSER_TOUCH_PERFORMANCE=1', #Common flag used for touch performance improvement
        'SBROWSER_FLING_END_NOTIFICATION_CANE=1', #Fling End Notification for Haptic Feedback in Cane Device
#        'SBROWSER_OMNIBOX_SMART_SUGGEST=1',    #Experimental omnibox suggest modifications
        'SBROWSER_ENABLE_RESET_DEFAULT_SETTINGS=1',	#Flag for Resetting settings to default
        'SBROWSER_GL_OUT_OF_MEMORY_PATCH=1',	#Flag for Patch to avoid GL_OUT_OF_MEMORY crash
        'SBROWSER_PAK_FILE_CHANGES=1',
        'SBROWSER_SCROLL_INPUTTEXT=1', #Flag for scrollable input box
        'SBROWSER_MULTIINSTANCE_DRAG_DROP=1', #Flag for multi instance drag & drop
        'SBROWSER_FONT_MEMORY_OPTIMIZATION=1',	#Flag for SBroswer Optimization
        'ENABLE_INPUT_TYPE_DATETIME_INCOMPLETE=1', #Flag to enable input type = datetime
        'SBROWSER_SEAMLESS_IFRAMES=1', #Flag for enabling seamless iFrames
        'SBROWSER_STYLE_SCOPED=1', #Flag for enabling style scoped.
        'SBROWSER_CUSTOM_HTTP_REQUEST_HEADER=1', #Flag for appending customized HTTP request header
        'SBROWSER_ENABLE_ECHO_PWD=1', #Flag to enable echo Password
        'SBROWSER_TEXT_ENCODING=1',  #Flag For text encoding
        'SBROWSER_ARROWKEY_FOCUS_NAVIGATION=1', #Focus movent with arrow keys
        'SBROWSER_DISABLE_PRECONNECTION_ON_BACKFORWARD=1', #disabling preconnection on back/forward action
        'SBROWSER_MEDIAPLAYER_MOTION_LISTENER=1', #Flag for enabling motion features for media player
        'SBROWSER_MEDIAPLAYER_AUDIOFOCUS_FIX=1', #Flag for additional implementation of audio focus logic
        'SBROWSER_MEDIAPLAYER_CURRENT_CONSUMPTION_FIX=1', #Flag for implementation for current consumption fix
        'SBROWSER_MEDIAPLAYER_POSTER_UPDATE_FIX=1', #Flag for poster image updating issue
        'SBROWSER_MEDIAPLAYER_SETVOLUME_IMPL=1', #Flag for additional implementation of SetVolume for volume/mute attributes of HTML5 video
        'SBROWSER_MEDIAPLAYER_BACKGROUND_PLAY_FIX=1', #Fix for MPSG100014719, Audio playing in background even if tab is not visible
        'SBROWSER_MEDIAPLAYER_LOOP=1', #Enable support for loop attribute of HTML5 A/V
        'SBROWSER_MEDIAPLAYER_MULTIINSTANCE_DRAG_N_DROP_SUPPORT=0', #Flag for multi-instance drag&drop support
        'SBROWSER_MEDIA_ADDITIONAL_MIMETYPE=1', #Flag for supporting more mime types for media playback
        'SBROWSER_MEDIA_ADDITIONAL_CODEC=1', #Flag for supporting more mime types for media playback
        'SBROWSER_HOVER_HIGHLIGHT=1', #Flag used for hover highlight
        'SBROWSER_POPUP_ZOOMER=1', # Flag for PopupZoomer
#       'SBROWSER_HIDE_URLBAR_WITHOUT_RESIZING=1', # Flag for HideUrlBar Feature without Resizing
        'SBROWSER_ENTERKEY_LONGPRESS=1', #Flag for showing context menu on EnterKey Longpress
#        'SBROWSER_FPAUTH_IMPL=0',		   #Flag used for autofill Finger print
#        'SBROWSER_FP_SUPPORT_PART2=0',  #PART4 implementation of Fingerprint Feature
        'SBROWSER_LOCALSTORAGE_LIMIT_PER_ORIGIN=1', #Flag for limitng localstorage to 5MB per origin
        'SBROWSER_MISSING_PLUGIN=1', #Flag for showing Missing plugin
#        'SBROWSER_IPC_SHARED_MEMORY_TRACKING=1', #Flag to track shared memory usagae and browser recovery when ashmmem exhausted
        'SBROWSER_SHAREDMEMORY_ERROR_LOGGING=1', #Flag to catch error code for shared memory allocation failure
        'SBROWSER_PHONENUMBER_DETECTION=1',		#Flag for Phonenumber detection
        'SBROWSER_IMIDEO_PREFERENCES=1',         #Flag for Imideo Service (Image ON)
        'SBROWSER_CONTENT_HIGHLIGHT=1', #Flag to enable highlight content which sends intent
        ############################ START : CSS Animation performance improvement ###################################
        'SBROWSER_CSS_ANIMATION_PERFORMANCE_IMPROVEMENT_1=1', #disable overlap flag off if animation condition in computeCompositingRequirement.
        'SBROWSER_CSS_ANIMATION_PERFORMANCE_IMPROVEMENT_2=1', #skip updateStyleIfNeeded in AnimationTimerFired.
#        'SBROWSER_CSS_ANIMATION_PERFORMANCE_IMPROVEMENT_3=1', #delay updateCompositingLayer in updateStyle.
        'SBROWSER_CSS_ANIMATION_PERFORMANCE_IMPROVEMENT_4=1', #skip startUpdateStyleIfNeededDispatcher in animaion.
        #'SBROWSER_CSS_ANIMATION_PERFORMANCE_IMPROVEMENT_5=1', #delay layout and updateStyle when animation fired.
        ############################ END : CSS Animation performance improvement ###################################
        'SBROWSER_USING_SAMSUNG_FONTS=1', #Flag for using Samsung fonts
        'SBROWSER_PASSWORD_ENCRYPTION=1', #Flag for password encryption
        'SBROWSER_SYSINFO_GETANDROIDBUILDPDA=0', #Flag for base::SysInfo::GetAndroidBuildPDA()
        'SBROWSER_SYSINFO_GETANDROIDCOUNTRYISO=1', #Flag for base::SysInfo::GetAndroidCountryISO()
        'SBROWSER_SYSINFO_GETANDROIDLANGUAGE=1', #Flag for base::SysInfo::GetAndroidLanguage()
        'SBROWSER_MULTIINSTANCE_VIDEO=1', # Flag to support multi instance video
        'SBROWSER_MULTIINSTANCE_TAB_DRAG_AND_DROP=0',          #Flag used for enabling multi instance tab drag and drop support in SBrowser
        'SBROWSER_IMELODY_ALLOW_DOWNLOAD=1',#Flag to allow imelody files to download rather than rendering in page
        'SBROWSER_ADJUST_FONT_COLOR=1', # Flag to adjust font color(RGB 8, 8, 8)
        'SBROWSER_QUICKLAUNCH_OBSERVER=1', #Flag for multiinstance quick launch observer
        'SBROWSER_DPAD_CENTER_KEY_HANDLING=1', # Flag to disable Handle DPAD_CENTER key handling
#        'SBROWSER_KEYLAGGING_PERFORMANCE=0', # Flag for keylagging performance
	'SBROWSER_RTSP_SUPPORT=1', #Flag to support rtsp scheme
        'SBROWSER_CQ_MPSG100014065=1', #Blank globe thumbnail without title is displayed in most visited page.
#        'SBROWSER_PROCESS_PER_TAB_FOR_CHROME_SCHEMA=1', # Avoid forcing chrome:schema to open in seperate renderer process.
        'SBROWSER_TABLET_SWITCH=1', # Engine flag to use in tablet mode
        'SBROWSER_ENABLE_JPN_SUPPORT_SUGGESTION_OPTION=1',
        'SBROWSER_UA_NEW_WINDOW_FROM_JS=1',
        'SBROWSER_ADJUST_WORKER_THREAD_PRIORITY=1', #adjust raster thread priority
#        'SBROWSER_POWER_SAVER_MODE_IMPL=1', #power saver mode changes
#        'SBROWSER_INCREASE_RASTER_THREAD=1', #increase the number of raster thread
        'SBROWSER_FOTA_UPDATE_ISSUE=1', #PLM - P131007-01263
        'SBROWSER_FLUSH_PENDING_MESSAGES=1',    # flush pending messages before initiating cleanups
#		'SBROWSER_ENABLE_APPNAP=1', #Flag used to enable App Nap feature
#        'SBROWSER_DOM_PERF_IMPROVEMENT=1',  # DocumentOrderedMap uses only one HashMap
#        'SBROWSER_QCOM_DOM_CACHED_SELECTOR=0',  # Results returned by Selector API are now cached
        'SBROWSER_CHANGE_TILESIZE=1', # Flag for change tile size of high dpi
#        'SBROWSER_READER_DEBUG=1',    # Reader Debug
        'SBROWSER_READER_NATIVE=1',    # Reader Native
        'SBROWSER_DISABLE_HISTOGRAM=1',  # Disable histograms. check no histograms are recorded in chrome://histograms
        'SBROWSER_HANDLE_MOUSECLICK_CTRL=1', # Flag to enable Mouse+Ctrl open link in new tab
        'SBROWSER_REDUCE_PRECONNECTION=1', # Reduce preconnection
#	'SBROWSER_KEYLAG=1', #Patch for keylagging issue in google homepage
#	'SBROWSER_KEYLAG_PATCH2=1', #flag to disable ubrk_openRules which is taking more time while inputting the first 				#char
            ],
		'conditions': [
		['enable_edge_gloweffect==1', {
		  'defines':[
			'SBROWSER_SUPPORT_GLOW_EDGE_EFFECT=0', # Flag for Edge Glow Effect
		   ],
		}],
		['enable_sbrowser_fingerprint==1', {
		  'defines':[
			'SBROWSER_FP_SUPPORT=0'  # Flag that enables fingerprint webpass feature for SBrowser
		   ],
		}],
		################################## kikin Modification Starts ########################################
		['enable_sbrowser_kikin==1', {
		   'defines':[
		       'SBROWSER_KIKIN=1'  # Flag that enables native-side kikin changes
		   ],
		}],
		################################## kikin Modification Ends ##########################################
		['enable_sbrowser_native_tex_upload==1', {
		   'defines':[
		       'SBROWSER_NATIVE_TEXTURE_UPLOAD=1'  # Flag that enables Tieto texture upload feature
		   ],
		}],
		################################## kikin Modification Ends ##########################################
		['enable_sbrowser_qc_vsync_patch==1', {
		   'defines':[
		       'SBROWSER_QC_VSYNC_PATCH=1'  # Flag that enables Qualcomm vsync patch code as provided by HQ
		   ],
		}],
		['enable_sbrowser_batching==1', {
		   'defines':[
		       'SBROWSER_BATCHING=1'  # Flag that enables batching
		   ],
		}],
  	        ################################## Bing Search Engine set via JS start ##########################################
                ['enable_sbrowser_bing_search_engine_set_via_javascript==1', {
                   'defines':[
                       'SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT=1',     # Flag Verizon bing search requirement
                   ],
                }],
  	        ################################## Bing Search Engine set via JS End ##########################################
		['enable_cac_feature == 1', {
      		   'defines':[
        		'SBROWSER_CAC=1',	# Flag for enabling CAC feature
      		   ],
    		}],
		################################### Flag used for Samsung Clipboard START ###################################
		['enable_sbrowser_clipboardex==1',{
	           'defines':[
			  'SBROWSER_CLIPBOARDEX_IMPL=1',
                   ],
		}],
                ################################### #Flag used for Samsung Clipboard END #######################################
	      ['enable_session_cache==1', {
		'defines':[
		  'SBROWSER_SESSION_CACHE=1',             # Flag for Session Cache Feature
		],
               }],
		###################### Enable webrtc feature: syam.dsp@samsung.com #################################
		['enable_sbrowser_webrtc==1', {
		   'defines':[
                       'SBROWSER_WEBRTC_FEATURE=1', # Added for sbrowser webrtc feature: syam.dsp@samsung.com
		],
	      }],
            ['enable_hide_urlbar==1', {
              'defines':[
                'SBROWSER_HIDE_URLBAR=1',             # Flag for HideUrlBar Feature
               ],
            }],
        ########## Enable CSC feature ##########
		['enable_csc_feature == 1', {
  		   'defines':[
    		'SBROWSER_CSC_FEATURE=1',          # Flag for enabling CSC feature
  		   ],
		}],
		['enable_angle_memory_feature==1', {
		 'defines':[
			'SBROWSER_ANGLE_MEMORY_FEATURE=1',
			],
		}],
		['disable_robustness_extension==1', {
		 'defines':[
			'SBROWSER_DISABLE_ROBUSTNESS_EXT=1',
			],
		}],
		########## Backported Features ##########		
		['disable_composited_antialiasing==1', {
		 'defines':[
			'SBROWSER_DISABLE_COMPOSITED_AA=1',
			],
		}],
		['enable_update_tile_pririty_transform_patch==1', {
		 'defines':[
			'SBROWSER_UPDATE_TILE_PRIORITY_PATCH=1',
			],
		}],		
		
		['enable_jpn_composing_region_feature == 1', {
		   'defines':[
				'ENABLE_JPN_COMPOSING_REGION=1',
		   ],
		}],
		['enable_jpn_composing_region_feature == 0', {
		   'defines':[
				'ENABLE_JPN_COMPOSING_REGION_DUMMY=1',
		   ],
		}],
        #### enable revocation check of certificate ####
		['enable_revocation_check==1', {
		  'defines':[
            'SBROWSER_CHECK_REVOKED_CERTIFICATE=1', # Flag to check whether a certificate is revoked or not
		   ],
		}],
		['enable_cookie_quick_flash == 1', {
		   'defines':[
				'SBROWSER_QUICK_COOKIE_FLASH = 1',
		   ],
		}],
		############ opera turbo solution ############
		['enable_data_compression == 1', {
		   'defines':[
				'SBROWSER_DATA_COMPRESSION = 1',
		   ],
		}],		
	],
	   }],
    ['enable_ui_composition_removal==1', {
      'defines':[
        'SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL=1' ,  # Flag for Ui composition removal
        'SBROWSER_GRAPHICS_GETBITMAP=1', #GetBitmap Feature flag
#		'SBROWSER_GRAPHICS_MSC_TOOL=1',  #MSC Tool Logs
		'SBROWSER_GRAPHICS_TLS_GLAPI=1', # TLS GLAPI For GLacess on Multiple Threads
		'SBROWSER_GRAPHICS_MAKE_VIR_CURRNT=1', # Issue 15841002: gpu: Fix corrupted state due to virtual MakeCurrent race.
      ],
    }],
    ['enable_qcom_v8 == 1', {
      'defines':[
        'SBROWSER_ENABLE_QCOMSO',	# Flag for Qcom V8 Feature
        'SBROWSER_QCOM_DOM_PREFETCH=1', # DOM optimization using prefetch by QCOM
      ],
    }],
    ['enable_sec_v8 == 1', {
      'defines':[
        'SBROWSER_ENABLE_SECSO',	# Flag for SEC V8 Feature
      ],
    }],
    ['enable_sbrowser_qcom_skia == 1', {
      'defines':[
        'SBROWSER_ENABLE_QCOMSKIA',	# Flag for Qcom skia
      ],
    }],
    ['enable_ndktoolchain_48!=1', {
      'defines':[
	 'SBROWSER_FIX_NDK48_ERRORS=1',
	],	
    }],
       ],

}
