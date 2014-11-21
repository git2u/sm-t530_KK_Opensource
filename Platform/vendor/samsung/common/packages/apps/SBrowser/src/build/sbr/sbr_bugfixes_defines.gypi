{
'conditions': [
    ['enable_sbrowser==1', {	   
          'defines':[						# Flag should used for defects fixes
			'SBROWSER_FOCUS_RING_FIX=1', #focus ring issue fix
			'SBROWSER_DISABLE_DATALIST_ELEMENT_CSS=0', #disable the default <datalist> style in css
			'SBROWSER_CQ_MPSG100011087=1',
			'SBROWSER_PLM_P130702_1591=1', #context menu issue fix
			'SBROWSER_PLM_P130705_0756=1', #facebook focus outline issue fix
			'SBROWSER_PLM_P130703_4926=1', #auto form fill fix
			'SBROWSER_PLM_P130704_0652=1', #application/vnd.wap.xhtml+xml support
			'SBROWSER_PLM_P130706_1198=1', # to fix edit field focus issue
			'SBROWSER_PLM_P130710_4354=1', # to fix find on page zoom issue
			'SBROWSER_PLM_P130712_4102=1', # to fix find on page on cancel issue
			'SBROWSER_PLM_P130731_5016=1', # to clear selection when handles are hidden while didstartload
			'SBROWSER_PLM_P131021_02958=1', # Media Control Play button not refreshed in some cases after loading of media file
			'SBROWSER_CQ_MPSG100011819=1', #to fix focus tap highlight
			'SBROWSER_CQ_RINGMARK=1', #enable enableInputTypeWeek for ringmark performance
			'SBROWSER_CQ_MPSG100011788=1', #Edit Field text selection fix
#			'SBROWSER_CQ_MPSG100012310=1', #to fix focus on naver.com
			'SBROWSER_CQ_MPSG100012331=1', #to Selection Handlers with no gap
			'SBROWSER_CQ_MPSG100012711=1', #selection popup doesnt come for iframe pages
			'SBROWSER_CQ_TAPHIGHLIGHT_FIX=1', #to fix tap highlight issue for mw.auction.co.kr
			'SBROWSER_CQ_MPSG100018638=1', #stress test crash due to invalid frame
			'SBROWSER_WEBAUDIO_GLITCH_FIX=1', #to fix glitch in WEBAUDIO feature
			'SBROWSER_BLOCK_UPDATING_INPUT_AT_TOUCHEND=1', #to fix to show IME when focus is in input box and flick occurs on m.naver.com
			'SBROWSER_FIX_DIRTY_CACHE_ISSUE=1', #to fix to set dirty cache is set as if it is not
			'SBROWSER_PLM_130814_05501=1',#do not download swf files if the request is from iframes during browsing.
			'SBROWSER_ENABLE_WEBAUDIO_DEFAULT=1', #to enable WEBAUDIO feature by default
			'SBROWSER_CQ_MPSG100013581=1', #crash fix while long press on .svg files  
			'SBROWSER_CQ_MPSG100013561=1',#Text Selection and handler mismatch/ edit field long press selection.
			'SBROWSER_PLM_P130827_05910=1', #Focus highlight not drwn properly
			'SBROWSER_WBMP_CORRUPTION_FIX=1', #wbmp image display corrption issue fix
			'SBROWSER_SEARCH_SCROLL_FIX=1', #scroll on suggestion list issue fix
			'SBROWSER_PLM_P130930_02960=1', #scroll bar should not participate in hittest 
			'SBROWSER_NAVER_POPUPZOOMER_FIX=1', #popupzoomer is coming while editing an article in cafe.naver.com (pc page)
			'SBROWSER_VOC_BACKFORWARD_NAVIGATION=1',# backforward navigation issue  in m.land.naver.com/article.
			'SBROWSER_PLM_P131209_05168=1', #non viewport pages in tablet changing pagescale after printing in landscape mode
			'SBROWSER_PLM_P131231_00002=1', #selection paint does not work when text is in composition mode in input box
			'SBROWSER_WRONG_PASSWORD_POPUPFIX=1', #To prevent popup in gmail.com when wrong credentials are entered.This may not work for sites like facebook.
#			'SBROWSER_WRONG_PASSWORD_FACEBOOKPOPUPFIX=1', #To prevent showing popup for form sudmission with the wrong password entered.
			'SBROWSER_CQ_MPSG100016511=1', #crash fix while loading mhtml file
			'SBROWSER_CSS_TOUCHMOVE_EVENT_FIX=1', #touchmove event doesn't work for certain div area with its visibility changed
			'SBROWSER_MEDIAPLAYER_WAKELOCK_FIX=1', #wakelock used to be set for both audio and video, but it's not supposed for audio
			'SBROWSER_PLM_P140128_09030=1', #Preventive null check for crash
			'SBROWSER_PLM_P140211_08572=1', #Preventive null check for crash
			'SBROWSER_PLM_P140212_00046=1', #Preventive null check for crash
			'SBROWSER_PLM_P140214_04362=1', #to correct text selection by spen in search field
			'SBROWSER_PLM_P140220_08626=1', #Preventive null check for crash
			'SBROWSER_CQ_KoreaHerald=1', #in m.koreaherald.com,button in iframe dosen't work
	            ],
		   }],
   ],

}
