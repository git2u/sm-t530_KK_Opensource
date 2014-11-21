
{
	'conditions': [
	['enable_session_cache==1', {
		'sources': [
			'session_cache_storage.cc',
			'session_cache_storage.h',
			'session_cache_interceptor.cc',
			'session_cache_interceptor.h',      
			'session_cache_request_handler.cc',
			'session_cache_request_handler.h',
			'session_cache_url_request_job.cc',
			'session_cache_url_request_job.h',
		],
       }],
	   ],
}

