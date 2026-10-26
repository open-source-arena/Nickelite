package tests

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"testing"
)

func TestHooks(t *testing.T) {
	if runtime.GOOS != "linux" || (runtime.GOARCH != "386" && runtime.GOARCH != "amd64" && runtime.GOARCH != "arm") {
		t.Skip("requires glibc Linux on x86, x86-64 or ARM32")
	}
	cc := os.Getenv("CC")
	if cc == "" {
		cc = "cc"
	}
	if _, err := exec.LookPath(cc); err != nil {
		t.Fatal(err)
	}
	src, err := filepath.Abs("..")
	if err != nil {
		t.Fatal(err)
	}
	dir := t.TempDir()
	if err := os.Mkdir(filepath.Join(dir, "imageformats"), 0700); err != nil {
		t.Fatal(err)
	}
	run := func(t *testing.T, env []string, name string, args ...string) {
		t.Helper()
		cmd := exec.Command(name, args...)
		cmd.Dir = dir
		cmd.Env = append(os.Environ(), env...)
		if out, err := cmd.CombinedOutput(); err != nil {
			t.Fatalf("%s %q: %v\n%s", name, args, err, out)
		}
	}
	build := func(output string, flags []string, files ...string) {
		t.Helper()
		args := []string{"-std=gnu11", "-Wall", "-Wextra", "-Werror", "-I" + src, "-DNH_VERSION=\"test\"", "-o", output}
		args = append(args, flags...)
		for _, file := range files {
			args = append(args, filepath.Join(src, file))
		}
		args = append(args, "-ldl", "-pthread")
		run(t, nil, cc, args...)
	}
	shared := []string{"-fPIC", "-shared"}
	build("libchain.so", append(shared, "-Wl,-z,lazy"), "tests/testdata/chain-library.c")
	build("libchain-now.so", append(shared, "-Wl,-z,relro,-z,now"), "tests/testdata/chain-library.c")
	// Initialization only needs a loadable libnickel fixture, not firmware.
	data, err := os.ReadFile(filepath.Join(dir, "libchain.so"))
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, "libnickel.so.1.0.0"), data, 0600); err != nil {
		t.Fatal(err)
	}
	for digit := 1; digit <= 3; digit++ {
		build(fmt.Sprintf("libhook%d.so", digit), append(shared, "-Wl,--wrap=mprotect", "-DCHAIN_DIGIT="+strconv.Itoa(digit)), "tests/testdata/chain-hook.c", "nh.c")
	}
	build("libpreload.so", shared, "tests/testdata/chain-preload.c")
	build("chain-check", nil, "tests/testdata/chain.c")
	build("imageformats/libfailure.so", shared, "tests/testdata/failure-hook.c", "nh.c")
	build("failure-check", nil, "tests/testdata/failure.c")
	t.Run("rollback", func(t *testing.T) { run(t, []string{"LD_LIBRARY_PATH=" + dir}, "./failure-check") })
	for _, relro := range []string{"0", "1"} {
		for _, now := range []string{"0", "1"} {
			for _, warm := range []string{"0", "1"} {
				for _, order := range []string{"123", "132", "213", "231", "312", "321"} {
					for _, preload := range []string{"0", "1"} {
						t.Run(fmt.Sprintf("relro=%s/binding=%s/warm=%s/order=%s/preload=%s", relro, now, warm, order, preload), func(t *testing.T) {
							value := ""
							if preload == "1" {
								value = filepath.Join(dir, "libpreload.so")
							}
							run(t, []string{"LD_PRELOAD=" + value}, "./chain-check", now, warm, order, preload, relro)
						})
					}
				}
			}
		}
	}
}
