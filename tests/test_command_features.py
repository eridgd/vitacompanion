import pathlib
import subprocess
import tempfile
import textwrap
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


def compile_and_run(source, support_sources):
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = pathlib.Path(tmpdir)
        source_path = tmp / "test.c"
        executable_path = tmp / "test"
        source_path.write_text(textwrap.dedent(source))
        subprocess.run(
            [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "src"),
                "-I",
                str(ROOT / "include"),
                str(source_path),
                *(str(ROOT / source_name) for source_name in support_sources),
                "-o",
                str(executable_path),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable_path)], check=True, cwd=ROOT)


class CommandFeatureTests(unittest.TestCase):
    def test_command_chains_accept_optional_trailing_semicolons(self):
        compile_and_run(
            r"""
            #include "parser.h"

            #include <stdlib.h>
            #include <string.h>

            int main(void)
            {
                char chain[] =
                    " press cross; wait 100ms; release cross; \r\n";
                char single[] = "version\n";
                char *commands[8] = {0};
                char *args[4] = {0};
                size_t count = 0;

                if (!parse_cmd_chain(chain, sizeof(chain) - 1,
                        commands, 8, &count) || count != 3)
                    return 1;

                if (parse_cmd(commands[0], strlen(commands[0]),
                        args, 4) != 2 ||
                    strcmp(args[0], "press") != 0 ||
                    strcmp(args[1], "cross") != 0)
                    return 2;

                memset(commands, 0, sizeof(commands));
                count = 0;
                if (!parse_cmd_chain(single, sizeof(single) - 1,
                        commands, 8, &count) || count != 1 ||
                    strcmp(commands[0], "version") != 0)
                    return 3;

                return 0;
            }
            """,
            ("src/parser.c",),
        )

    def test_command_chain_limit_is_reported(self):
        compile_and_run(
            r"""
            #include "parser.h"

            int main(void)
            {
                char chain[] = "version;version\n";
                char *commands[1] = {0};
                size_t count = 0;

                return parse_cmd_chain(chain, sizeof(chain) - 1,
                    commands, 1, &count) ? 1 : 0;
            }
            """,
            ("src/parser.c",),
        )

    def test_wait_units_are_milliseconds_and_seconds_only(self):
        compile_and_run(
            r"""
            #include "parser.h"

            #include <stdint.h>

            static int expect_valid(const char *value, uint32_t expected)
            {
                uint32_t actual = 0;
                return !parse_wait_duration_ms(value, &actual) ||
                    actual != expected;
            }

            int main(void)
            {
                uint32_t value;

                if (expect_valid("1000ms", 1000) ||
                    expect_valid("3s", 3000) ||
                    expect_valid("0ms", 0))
                    return 1;

                if (parse_wait_duration_ms("5f", &value) ||
                    parse_wait_duration_ms("1000", &value) ||
                    parse_wait_duration_ms("1.5s", &value) ||
                    parse_wait_duration_ms("3S", &value) ||
                    parse_wait_duration_ms("4294968s", &value))
                    return 2;

                return 0;
            }
            """,
            ("src/parser.c",),
        )

    def test_input_parser_covers_buttons_sticks_touches_and_reset(self):
        compile_and_run(
            r"""
            #include <vitacompanion_input.h>

            #include <stdint.h>

            int main(void)
            {
                vitacompanion_input_action action;
                char *cross[] = {"press", "cross"};
                char *stick[] = {"press", "left-stick", "0", "255"};
                char *touch[] = {
                    "press", "front-touch", "3", "1919", "1087"
                };
                char *release_touch[] = {
                    "release", "front-touch", "3"
                };
                char *release_all[] = {"release", "all"};
                char *bad_stick[] = {
                    "press", "right-stick", "256", "0"
                };
                char *bad_touch[] = {
                    "press", "rear-touch", "4", "0", "0"
                };

                if (vitacompanion_parse_press(cross, 2, &action) < 0 ||
                    action.type != VITACOMPANION_INPUT_BUTTON ||
                    action.data.button.mask != 0x00004000 ||
                    !action.active)
                    return 1;

                if (vitacompanion_parse_press(stick, 4, &action) < 0 ||
                    action.type != VITACOMPANION_INPUT_ANALOG ||
                    action.data.analog.stick != VITACOMPANION_STICK_LEFT ||
                    action.data.analog.x != 0 ||
                    action.data.analog.y != 255)
                    return 2;

                if (vitacompanion_parse_press(touch, 5, &action) < 0 ||
                    action.type != VITACOMPANION_INPUT_TOUCH ||
                    action.data.touch.slot != 3 ||
                    action.data.touch.x != 1919 ||
                    action.data.touch.y != 1087)
                    return 3;

                if (vitacompanion_parse_release(
                        release_touch, 3, &action) < 0 ||
                    action.type != VITACOMPANION_INPUT_TOUCH ||
                    action.active)
                    return 4;

                if (vitacompanion_parse_release(
                        release_all, 2, &action) < 0 ||
                    action.type != VITACOMPANION_INPUT_RESET)
                    return 5;

                if (vitacompanion_parse_press(
                        bad_stick, 4, &action) >= 0 ||
                    vitacompanion_parse_press(
                        bad_touch, 5, &action) >= 0)
                    return 6;

                return 0;
            }
            """,
            ("src/input_parse.c",),
        )

    def test_version_comes_from_loaded_module_metadata(self):
        source = (ROOT / "src" / "version.c").read_text()
        self.assertIn("sceKernelGetModuleIdByAddr", source)
        self.assertIn("sceKernelGetModuleInfo", source)
        self.assertRegex(
            source,
            r"info\.modver\[1\].*info\.modver\[0\]",
        )

    def test_quit_replaces_kill_and_destroy_commands(self):
        source = (ROOT / "src" / "cmd_definitions.c").read_text()
        self.assertIn('{.name = "quit"', source)
        self.assertIn('strcmp(arg_list[1], "all")', source)
        self.assertNotIn('{.name = "kill"', source)
        self.assertNotIn('{.name = "destroy"', source)

    def test_kernel_touch_contacts_keep_stable_ids(self):
        source = (ROOT / "kernel" / "main.c").read_text()
        self.assertIn("SYNTHETIC_TOUCH_ID_BASE + slot", source)
        self.assertIn("current->reportNum++", source)
        self.assertIn("touch_states[port][slot].active", source)

    def test_input_requires_the_expected_kernel_api(self):
        compile_and_run(
            r"""
            #include "input.h"
            #include <vitacompanion_kernel.h>

            static int api_version = -1;
            static int api_version_call_count;
            static int reset_count;

            int vitaCompanionKernelGetApiVersion(void)
            {
                ++api_version_call_count;
                return api_version;
            }

            int vitaCompanionKernelSetButtons(
                uint32_t buttons, int pressed)
            {
                (void)buttons;
                (void)pressed;
                return 0;
            }

            int vitaCompanionKernelSetAnalog(
                int stick, int x, int y, int active)
            {
                (void)stick;
                (void)x;
                (void)y;
                (void)active;
                return 0;
            }

            int vitaCompanionKernelSetTouch(
                int port, int slot, int x, int y, int active)
            {
                (void)port;
                (void)slot;
                (void)x;
                (void)y;
                (void)active;
                return 0;
            }

            int vitaCompanionKernelReset(void)
            {
                ++reset_count;
                return 0;
            }

            int main(void)
            {
                if (input_start() >= 0 ||
                    input_is_ready() ||
                    api_version_call_count != 1)
                    return 1;

                input_end();
                if (reset_count != 0)
                    return 2;

                api_version = VITACOMPANION_KERNEL_ABI_VERSION;
                if (input_start() < 0 ||
                    !input_is_ready() ||
                    api_version_call_count != 2)
                    return 3;

                input_end();
                if (input_is_ready() || reset_count != 1)
                    return 4;

                return 0;
            }
            """,
            ("src/input.c",),
        )

    def test_kernel_module_is_a_required_taihen_dependency(self):
        input_source = (ROOT / "src" / "input.c").read_text()
        cmake_source = (ROOT / "CMakeLists.txt").read_text()
        readme = (ROOT / "README.md").read_text()

        self.assertNotIn("taiLoadStartKernelModule", input_source)
        self.assertNotIn("_vshKernelSearchModuleByName", input_source)
        self.assertNotIn("VITACOMPANION_KERNEL_PATH", cmake_source)
        self.assertIn(
            "${CMAKE_CURRENT_BINARY_DIR}/libvitacompanion_kernel_stub.a",
            cmake_source,
        )
        self.assertNotIn(
            "libvitacompanion_kernel_stub_weak.a",
            cmake_source,
        )
        self.assertIn(
            "*KERNEL\nur0:tai/vitacompanion_kernel.skprx",
            readme,
        )
        self.assertIn(
            "The kernel module is required",
            readme,
        )


if __name__ == "__main__":
    unittest.main()
