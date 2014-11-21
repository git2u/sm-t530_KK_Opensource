
{
	'sources': [
	   
	   
	   '../browser/sbr_device_motion/sbr_data_fetcher.h',
	   '../browser/sbr_device_motion/sbr_device_motion_android.cc',
           '../browser/sbr_device_motion/sbr_device_motion_android.h',
	   '../browser/sbr_device_motion/sbr_motion_message_filter.cc',
           '../browser/sbr_device_motion/sbr_motion_message_filter.h',
           '../browser/sbr_device_motion/sbr_motion.h',
           '../browser/sbr_device_motion/sbr_provider.cc',
           '../browser/sbr_device_motion/sbr_provider.h',
           '../browser/sbr_device_motion/sbr_provider_impl.cc',
           '../browser/sbr_device_motion/sbr_provider_impl.h',	 
           '../renderer/sbr_device_motion_dispatcher.cc',
           '../renderer/sbr_device_motion_dispatcher.h',
	   
	],
	'conditions': [
	['enable_edge_gloweffect==1', {
		'sources': [
	        '../browser/android/overscroll_glow.cc',
			'../browser/android/overscroll_glow.h',
			'../browser/android/edge_effect.cc',
			'../browser/android/edge_effect.h',
		],
       }],
	   ],
}
