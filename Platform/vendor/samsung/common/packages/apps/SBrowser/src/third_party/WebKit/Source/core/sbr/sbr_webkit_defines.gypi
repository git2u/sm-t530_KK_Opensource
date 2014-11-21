
{
        'feature_defines': [
                'ENABLE_CSS_REGION_OVERFLOW_TO_FRAGMENT=1',
                'ENABLE_SBROWSER_RINGMARK=1',
                'SBROWSER_WML=1',   #Engine side WML support changes
                'SBROWSER_WBMP_SUPPORT=1', #support wbmp images
                'SBROWSER_CANVAS_MULTIPLE_PATHS=1', #enable HTML5 multiple canvas paths feature
#               'SBROWSER_REDUCE_IMAGE_DECODING=1', #reduce the number of times to decoding bmp image
                'SBROWSER_BLEND_SUPPORT=1', # HTML5 Canvas Blend Support.
                'SBROWSER_SPATIAL_NAVIGATION_SUPPORT=1',
                'SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT=1', #support for bing search setting from JS
                'SBROWSER_TEXT_AUTOSIZER=1', #SBrowser font boosting flag
#               'ENABLE_ACCELERATED_OVERFLOW_SCROLLING=1', #enabling overflow scrolling for ringmark performance'
                'SBROWSER_DOM_TIMEOUT_CORRECTION=1', #correct dom timeout
                'SBROWSER_CQ_MPSG100014509=1',#to add WEBAUDIO exception handling
                'SBROWSER_RENDERTEXTAREA_DNC_SCROLLBAR=1', #Do not consider scrollbar width while rendering text area
                'SBROWSER_DISABLE_LOAD_FOR_EMBED_MIMETYPES=1', #Disable loading for embed elements with unsupported codecs inorder to avoid sending download request currently done for audio/mpeg mime type
        ],
        'conditions': [
			['enable_data_compression == 1', {
				'defines':[
					'SBROWSER_DATA_COMPRESSION = 1',
				],
			}],	
        ],
}

