
#ifndef CHROME_BROWSER_ANDROID_SBR_PASSWORDMANAGER_SBR_PASSWORDMANAGER_H_
#define CHROME_BROWSER_ANDROID_SBR_PASSWORDMANAGER_SBR_PASSWORDMANAGER_H_

#include <jni.h>
#include "base/android/jni_helper.h"
#include "chrome/browser/password_manager/login_database.h"

class LoginDatabase;

class SbrPasswordManagerUtility
{
public :
	 enum dbChangeType 
 	 {
 		addOperation,
 		updateOperation,
 		deleteOperation,
 		deleteDBOperation
 	 };
	  SbrPasswordManagerUtility(JNIEnv* env, jobject obj);
	  void Destroy(JNIEnv * env, jobject obj);
	  static SbrPasswordManagerUtility*  getSbrPasswdMgrUtility();
	  //void setLoginDB(LoginDatabase* login_db);
	  bool getEncryptedPassword(const string16& plain_text,std::string* cipher_text);
	  bool getDecryptedPassword(const std::string& cipher_text,string16* plain_text);
	  void notifyDBChange(const string16 & username ,const std::string & url,dbChangeType type);

private:
	
	~SbrPasswordManagerUtility();
		
		
	
	//LoginDatabase* mLoginDb;
	JavaObjectWeakGlobalRef weak_java_ref;

	DISALLOW_COPY_AND_ASSIGN(SbrPasswordManagerUtility);

};
// Registers the SbrPasswordManagerUtility native method.
bool RegisterSbrPasswordManagerUtility(JNIEnv* env);
#endif
