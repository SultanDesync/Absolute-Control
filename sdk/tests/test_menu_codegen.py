import copy
import importlib.util
import pathlib
import subprocess
import sys
import tempfile
import unittest


SDK = pathlib.Path(__file__).resolve().parents[1]
TOOL_PATH = SDK / "tools" / "menu_codegen.py"
EXAMPLE = SDK / "examples" / "absolute-head-tracking.menu.json"
GENERATED = SDK / "examples" / "generated" / "AbsoluteHeadTrackingMenu.generated.h"
FIXTURES = SDK / "tests" / "fixtures"

spec = importlib.util.spec_from_file_location("menu_codegen", TOOL_PATH)
menu_codegen = importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(menu_codegen)


class MenuCodegenTests(unittest.TestCase):
    def test_representative_definition_validates(self):
        data = menu_codegen.load(EXAMPLE)
        self.assertEqual(data["module"]["id"], "absolute.head_tracking")
        self.assertEqual(len(data["pages"]), 3)
        axes = next(page for page in data["pages"] if page["id"] == "axes")
        self.assertEqual(
            sum(len(section["options"]) for section in axes["sections"]),
            12,
        )

    def test_generation_is_deterministic_and_checked_in(self):
        data = menu_codegen.load(EXAMPLE)
        first = menu_codegen.generate(data, EXAMPLE.name)
        second = menu_codegen.generate(data, EXAMPLE.name)
        self.assertEqual(first, second)
        self.assertEqual(GENERATED.read_text(encoding="utf-8"), first)
        self.assertIn("ParseControlId", first)
        self.assertIn("MakePages", first)
        self.assertIn("ControlKind::GroupHeader", first)
        self.assertIn("ReassignBindingCallback reassignBinding", first)
        self.assertIn("pages[0].reassignBinding = provider.reassignBinding", first)

    def test_inline_actions_generate_and_non_actions_reject_flag(self):
        valid = menu_codegen.load(EXAMPLE)
        candidate = copy.deepcopy(valid)
        first_action = candidate["pages"][2]["sections"][0]["options"][1]
        first_action["flags"] = ["layoutInline"]
        second_action = copy.deepcopy(first_action)
        second_action["id"] = "bindings.reset"
        second_action["label"] = "Reset tracking"
        candidate["pages"][2]["sections"][0]["options"].append(second_action)
        validated = menu_codegen.validate(candidate)
        rendered = menu_codegen.generate(validated, "inline.menu.json")
        structured = rendered.split("kBindingsControls", 1)[1].split(
            "kBindingsLegacyControls", 1)[0]
        legacy = rendered.split("kBindingsLegacyControls", 1)[1].split(
            "struct ProviderCallbacks", 1)[0]
        self.assertIn("kControlLayoutInline", structured)
        self.assertNotIn("kControlLayoutInline", legacy)

        invalid = copy.deepcopy(valid)
        invalid["pages"][0]["sections"][0]["options"][0]["flags"] = [
            "layoutInline"
        ]
        with self.assertRaisesRegex(
            menu_codegen.ValidationError,
            "layoutInline is supported only for actions",
        ):
            menu_codegen.validate(invalid)

    def test_forbidden_presentation_properties_fail(self):
        with self.assertRaisesRegex(menu_codegen.ValidationError, "unknown properties: color"):
            menu_codegen.load(FIXTURES / "invalid-coordinate.menu.json")

        valid = menu_codegen.load(EXAMPLE)
        for forbidden in ("x", "color", "script", "css"):
            candidate = copy.deepcopy(valid)
            candidate["pages"][0]["sections"][0]["options"][0][forbidden] = "forbidden"
            with self.subTest(forbidden=forbidden):
                with self.assertRaisesRegex(menu_codegen.ValidationError, f"unknown properties: {forbidden}"):
                    menu_codegen.validate(candidate)

    def test_duplicate_callback_id_fails(self):
        with self.assertRaisesRegex(menu_codegen.ValidationError, "unique across the module"):
            menu_codegen.load(FIXTURES / "invalid-duplicate-control.menu.json")

    def test_invalid_bounds_fail(self):
        with self.assertRaisesRegex(menu_codegen.ValidationError, "minimum must be less"):
            menu_codegen.load(FIXTURES / "invalid-bounds.menu.json")

    def test_module_control_capacity_fails_before_generation(self):
        valid = menu_codegen.load(EXAMPLE)
        candidate = copy.deepcopy(valid)
        template = candidate["pages"][0]["sections"][0]["options"][0]
        candidate["pages"] = []
        for page_index in range(5):
            options = []
            option_count = 127 if page_index < 4 else 1
            for control_index in range(option_count):
                option = copy.deepcopy(template)
                option["id"] = f"value.{page_index}.{control_index}"
                options.append(option)
            candidate["pages"].append({
                "id": f"page.{page_index}",
                "title": f"Page {page_index}",
                "sections": [{
                    "id": "main",
                    "title": "Main",
                    "options": options,
                }],
            })
        with self.assertRaisesRegex(
            menu_codegen.ValidationError,
            "module exceeds the ABI-v1 limit of 512 controls",
        ):
            menu_codegen.validate(candidate)

    def test_check_mode_detects_stale_output(self):
        with tempfile.TemporaryDirectory() as directory:
            stale = pathlib.Path(directory) / "stale.h"
            stale.write_text("stale", encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(TOOL_PATH), "generate", str(EXAMPLE), str(stale), "--check"],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 1)
            self.assertIn("stale", result.stderr)


if __name__ == "__main__":
    unittest.main()
