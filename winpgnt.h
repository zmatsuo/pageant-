
#ifndef _WINPGNT_H_
#define	_WINPGNT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

bool winpgnt_start();
void winpgnt_stop();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

#include <vector>
class KeyListItem {
public:
	int no;
	std::string name;
	std::string comment;
};

std::vector<KeyListItem> keylist_update2();
#endif

#endif	// _WINPGNT_H_

// Local Variables:
// mode: c++
// coding: utf-8
// tab-width: 4
// End:
