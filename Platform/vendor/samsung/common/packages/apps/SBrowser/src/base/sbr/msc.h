#ifndef MSC_H
#define MSC_H

#define MSC_ENABLE 1
#define MSC_DATA_ENABLE 0

#ifdef MSC_ENABLE
#define MSC_MESSAGE_NORMAL(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< "->" <<  destination << " [ label = \"" << label <<" "<< Data<<"\" , linecolor=\"red\" ] ;"
#define MSC_MESSAGE(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< "->" <<  destination << " [ label = \"" << label <<" "<<this<<","<< Data<<"\" , linecolor=\"red\" ] ;"

#define MSC_MESSAGE_2(source,destination,label,Data,Data2) \
	LOG(INFO)<<"MSCGEN^"<<source<< "->" <<  destination << " [ label = \"" << label <<" "<<this<<","<< Data <<"," << Data2 <<"\" , linecolor=\"red\" ] ;"

#define MSC_METHOD(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< "=>" <<  destination << " [ label = \"" << label <<" "<<this<<","<< Data<<"\" ] ;"
#define MSC_METHOD_NORMAL(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< "=>" <<  destination << " [ label = \"" << label <<" "<< Data<<"\" ] ;"


#define MSC_RETURN(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< ">>" <<  destination << " [ label = \"" << label <<" "<<this<<","<< Data<<"\" ] ;"


#define MSC_CALLBACK(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< "=>>" <<  destination << " [ label = \"" << label <<" "<<this<<","<< Data<<"\" , linecolor=\"blue\" ] ;"
	
#define MSC_CALLBACK_2(source,destination,label,Data,Data2) \
	LOG(INFO)<<"MSCGEN^"<<source<< "=>>" <<  destination << " [ label = \"" << label <<" "<<this<<","<< Data<<","<<Data2<<"\" , linecolor=\"blue\" ] ;"
	
#ifdef MSC_DATA_ENABLE
	#define MSC_DATA_METHOD(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< "=>" <<  destination << " [ label = \"" << label <<" "<<this<<","<< Data<<"\" ] ;"
	
	#define MSC_DATA_CALLBACK(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< "=>>" <<  destination << " [ label = \"" << label <<" "<<this<<","<< Data<<"\" , linecolor=\"blue\" ] ;"

	#define MSC_DATA_CALLBACK_NORMAL(source,destination,label,Data) \
	LOG(INFO)<<"MSCGEN^"<<source<< "=>>" <<  destination << " [ label = \"" << label <<","<< Data<<"\" , linecolor=\"blue\" ] ;"
#else
	#define MSC_DATA_METHOD(source,destination,label,Data)
	#define MSC_DATA_CALLBACK(source,destination,label,Data)
	#define MSC_DATA_CALLBACK_NORMAL(source,destination,label,Data)
#endif

#define MSC_NOTE(note) \
	LOG(INFO)<<"MSCGEN^"<<"--- [ label = \"" <<  note << " \" ];"
#define MSC_NOTE_DATA(note,data) \
	LOG(INFO)<<"MSCGEN^"<<"--- [ label = \"" <<  note <<" , "<<data<< " \" ];"
#else
#define MSC_MESSAGE(source,destination,label,Data)
#define MSC_MESSAGE_NORMAL(source,destination,label,Data)
#define MSC_MESSAGE_2(source,destination,label,Data.Data2)

#define MSC_METHOD(source,destination,label,Data)
#define MSC_METHOD_NORMAL(source,destination,label,Data)

#define MSC_RETURN(source,destination,label,Data)

#define MSC_CALLBACK(source,destination,label,Data)
#define MSC_CALLBACK_2(source,destination,label,Data,Data2)

#define MSC_NOTE(note)
#define MSC_NOTE_DATA(note,data)

#define MSC_DATA_METHOD(source,destination,label,Data)
#define MSC_DATA_CALLBACK(source,destination,label,Data)
#define MSC_DATA_CALLBACK_NORMAL(source,destination,label,Data)

#endif

#endif
