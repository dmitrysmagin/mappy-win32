/* Standalone TSDF/TSIN round-trip test. Build from src/:
 *   gcc -O2 -Wall -Wextra -std=gnu99 -o tileset_test.exe tileset_test.c tileset.c
 */
#include <stdio.h>
#include "tileset.h"

int main(void)
{
	int rc = TS_SelfTest();
	if (rc != 0) {
		printf("TS_SelfTest failed: %d\n", rc);
		return 1;
	}
	printf("TS_SelfTest ok\n");
	return 0;
}
