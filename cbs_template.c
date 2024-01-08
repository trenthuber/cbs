#define CBS_IMPLEMENTATION
#include "cbs.h"

int main(int argc, char **argv) {
	cbs_rebuild_self(argv);
	cbs_shift_args(&argc, &argv);

	// Build script goes here

	return 0;
}
