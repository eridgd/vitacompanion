import pathlib
import subprocess
import tempfile
import textwrap
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
VENDOR = ROOT / "vendor" / "libftpvita"


def compile_and_run(source, support_sources=("ftpvita_path.c",)):
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = pathlib.Path(tmpdir)
        source_path = tmp / "test.c"
        exe_path = tmp / "test"
        source_path.write_text(textwrap.dedent(source))
        subprocess.run(
            [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(VENDOR),
                str(source_path),
                *(str(VENDOR / name) for name in support_sources),
                "-o",
                str(exe_path),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(exe_path)], check=True, cwd=ROOT)


class LibftpvitaCompatTests(unittest.TestCase):
    def test_vita_paths_are_normalized_for_file_commands(self):
        compile_and_run(
            r"""
            #include "ftpvita_path.h"

            #include <stdio.h>
            #include <stdlib.h>
            #include <string.h>

            static void expect_path(const char *cur_path, const char *arg, const char *expected)
            {
                char actual[256];
                ftpvita_path_from_command_arg(cur_path, arg, actual, sizeof(actual));
                if (strcmp(actual, expected) != 0) {
                    fprintf(stderr, "for arg %s expected %s, got %s\n", arg, expected, actual);
                    exit(1);
                }
            }

            int main(void)
            {
                expect_path("/", "/ux0:/data/bootstrap.self\r\n", "/ux0:/data/bootstrap.self");
                expect_path("/", "ux0:/data/bootstrap.self\r\n", "/ux0:/data/bootstrap.self");
                expect_path("/", "uma0:/media/file.bin\r\n", "/uma0:/media/file.bin");
                expect_path("/ux0:/data", "bootstrap.self\r\n", "/ux0:/data/bootstrap.self");
                expect_path("/ux0:/", "data/bootstrap.self\r\n", "/ux0:/data/bootstrap.self");
                return 0;
            }
            """
        )

    def test_list_paths_are_normalized_and_list_options_are_ignored(self):
        compile_and_run(
            r"""
            #include "ftpvita_path.h"

            #include <stdio.h>
            #include <stdlib.h>
            #include <string.h>

            static void expect_list_path(const char *cur_path, const char *arg, const char *expected)
            {
                char actual[256];
                ftpvita_path_from_list_args(cur_path, arg, actual, sizeof(actual));
                if (strcmp(actual, expected) != 0) {
                    fprintf(stderr, "for list arg %s expected %s, got %s\n", arg, expected, actual);
                    exit(1);
                }
            }

            int main(void)
            {
                expect_list_path("/ux0:/data", "", "/ux0:/data");
                expect_list_path("/", "ux0:/data/\r\n", "/ux0:/data/");
                expect_list_path("/", "/ux0:/data/\r\n", "/ux0:/data/");
                expect_list_path("/", "-la ux0:/data/\r\n", "/ux0:/data/");
                expect_list_path("/ux0:/data", "-a\r\n", "/ux0:/data");
                expect_list_path("/ux0:/", "data/\r\n", "/ux0:/data/");
                return 0;
            }
            """
        )

    def test_epsv_response_uses_extended_passive_mode_format(self):
        compile_and_run(
            r"""
            #include "ftpvita_path.h"

            #include <stdio.h>
            #include <stdlib.h>
            #include <string.h>

            int main(void)
            {
                char actual[128];
                ftpvita_format_epsv_response(actual, sizeof(actual), 65000);
                if (strcmp(actual, "229 Entering Extended Passive Mode (|||65000|)\r\n") != 0) {
                    fprintf(stderr, "unexpected EPSV reply: %s\n", actual);
                    exit(1);
                }
                return 0;
            }
            """
        )

    def test_mdtm_response_uses_ftp_timestamp_format(self):
        compile_and_run(
            r"""
            #include "ftpvita_path.h"

            #include <stdio.h>
            #include <stdlib.h>
            #include <string.h>

            int main(void)
            {
                char actual[128];
                ftpvita_format_mdtm_response(actual, sizeof(actual), 2026, 7, 4, 9, 5, 3);
                if (strcmp(actual, "213 20260704090503\r\n") != 0) {
                    fprintf(stderr, "unexpected MDTM reply: %s\n", actual);
                    exit(1);
                }
                return 0;
            }
            """
        )

    def test_ftp_command_table_wires_compatibility_commands(self):
        source = (VENDOR / "ftpvita.c").read_text()
        self.assertIn("ftpvita_path_from_list_args", source)
        self.assertIn("add_entry(EPSV)", source)
        self.assertIn("add_entry(NLST)", source)
        self.assertIn("add_entry(MDTM)", source)

    def test_commands_without_argument_separator_have_empty_args(self):
        source = (VENDOR / "ftpvita.c").read_text()
        self.assertIn('client->recv_cmd_args = "";', source)
        self.assertNotIn("client->recv_cmd_args = client->recv_buffer;", source)

    def test_send_all_handles_partial_writes_and_disconnects(self):
        compile_and_run(
            r"""
            #include "ftpvita_io.h"

            #include <stdlib.h>
            #include <string.h>

            static const char *expected;
            static unsigned int offset;
            static int fail_after;
            static int zero_after;

            static int mock_send(int socket, const void *buffer, unsigned int length, int flags)
            {
                unsigned int amount;
                (void)socket;
                (void)flags;

                if (fail_after >= 0 && (int)offset >= fail_after)
                    return -123;
                if (zero_after >= 0 && (int)offset >= zero_after)
                    return 0;

                amount = length > 2 ? 2 : length;
                if (memcmp(buffer, expected + offset, amount) != 0)
                    exit(10);
                offset += amount;
                return (int)amount;
            }

            int main(void)
            {
                expected = "abcdef";
                offset = 0;
                fail_after = -1;
                zero_after = -1;
                if (ftpvita_send_all(mock_send, 1, expected, 6, 0) != 6 || offset != 6)
                    return 1;

                offset = 0;
                fail_after = 2;
                if (ftpvita_send_all(mock_send, 1, expected, 6, 0) != -123 || offset != 2)
                    return 2;

                offset = 0;
                fail_after = -1;
                zero_after = 2;
                if (ftpvita_send_all(mock_send, 1, expected, 6, 0) >= 0 || offset != 2)
                    return 3;

                return 0;
            }
            """,
            support_sources=("ftpvita_io.c",),
        )

    def test_disconnect_paths_are_bounded_and_shutdown_does_not_wait_under_mutex(self):
        source = (VENDOR / "ftpvita.c").read_text()
        self.assertIn("SCE_NET_SO_SNDTIMEO", source)
        self.assertIn("SCE_NET_SO_RCVTIMEO", source)
        self.assertIn("client_send_data_msg(client, buffer) < 0", source)
        self.assertIn('"%15s"', source)
        self.assertIn("sizeof(client->recv_buffer) - 1", source)
        self.assertIn("if (server_stopping)", source)
        self.assertIn("Server accept error:", source)

        shutdown_start = source.index("static void client_list_thread_end()")
        shutdown_end = source.index("static int client_thread", shutdown_start)
        shutdown = source[shutdown_start:shutdown_end]
        self.assertLess(
            shutdown.index("sceKernelUnlockMutex(client_list_mtx, 1)"),
            shutdown.index("sceKernelWaitThreadEnd(it->thid"),
        )


if __name__ == "__main__":
    unittest.main()
