// defkey.h
// Programmed by T.Maruyama

#define KEY_MATRIX_BANK_MAX    16                    /* キーマトリクスの最大定義数 */
#define SECTION_NAME_MAX    64                    /* セクション名の最大長 */

typedef struct
{
	int flag;															/* define flag */
	BYTE name[SECTION_NAME_MAX];										/* section name */

} TDEFKEY_SECTION;

int get_keymat_max(void);
UINT8 * get_keymattbl_ptr(void);

int init_defkey(void);
int end_defkey(void);

int read_defkey(void);
