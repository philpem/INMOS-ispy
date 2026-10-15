"""Host checks for the Acorn export and ISpy template-input selection.

These use GCC and a link stub; they do not validate Acorn object generation
or communicate with a transputer.
"""

import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent


class NativeExport(unittest.TestCase):
    def test_makefile_dependencies_match_export(self):
        spec = importlib.util.spec_from_file_location(
            'export_riscos', ROOT / 'tools/export-riscos.py')
        exporter = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(exporter)
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / 'export'
            exporter.export(ROOT, destination)
            macros, rules = {}, {}
            for line in (destination / 'Makefile').read_text().replace('\\\n', ' ').splitlines():
                if not line.strip() or line.startswith('#') or line[0].isspace():
                    continue
                if '=' in line and ':' not in line.split('=', 1)[0]:
                    name, value = line.split('=', 1)
                    macros[name.strip()] = value.strip()
                else:
                    target, dependencies = line.split(':', 1)
                    rules[target.strip()] = dependencies

            visited = set()

            def visit(target):
                if target in visited:
                    return
                visited.add(target)
                if target.startswith('@.'):
                    file = destination / target[2:].replace('.', '/')
                    if file.is_file():
                        return
                if ':' in target:  # externally installed Acorn/OSLib inputs
                    return
                self.assertIn(target, rules, 'Missing file or build rule')
                expanded = re.sub(r'\$\(([^)]+)\)',
                                  lambda match: macros[match.group(1)], rules[target])
                for dependency in expanded.split():
                    visit(dependency)

            visit('all')
            self.assertEqual({name for name in visited if name.startswith('@.c.')},
                             {'@.c.' + name for name in (
                                 'check', 'cklib', 'mtest', 'ftest', 'ro_link', 'ltest', 'netcfg',
                                 'testt2', 'testt32', 'testt414', 'testt425',
                                 'testt80x', 'testt800', 'testt801', 'testlater')})
            self.assertIn('@.s.tmblink', visited)
            self.assertIn('3/26bit', macros['CCflags'])


@unittest.skipUnless(shutil.which('gcc'), 'GCC required for host-only checks')
class TemplateInput(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temporary.name)
        stub = cls.directory / 'linkstub.c'
        stub.write_text('''#include <stdio.h>
#include <stdlib.h>
#include "inmos.h"
#include "cklib.h"
LINK OpenLink(char *name) { printf("MOCK OPEN %s\\n", name); exit(0); }
int CloseLink(LINK id) { return 1; }
int ReadLink(LINK id, unsigned char *buf, unsigned int count, int timeout) { abort(); return 0; }
int WriteLink(LINK id, unsigned char *buf, unsigned int count, int timeout) { abort(); return 0; }
int ResetLink(LINK id) { abort(); return 0; }
int AnalyseLink(LINK id) { abort(); return 0; }
int TestError(LINK id) { abort(); return 0; }
''')
        for mode, flags in [('native', ['-DRISCOS']),
                            ('unix', ['-DDEFAULTLINK="unix-default"'])]:
            subprocess.run(['gcc', '-std=c90', '-pedantic-errors', '-I' + str(ROOT),
                            *flags, str(ROOT / 'check.c'), str(ROOT / 'cklib.c'),
                            str(stub), '-o', str(cls.directory / mode)], check=True)
        cls.template = cls.directory / 'network'
        cls.template.write_text('Using template-link ispy 3.23\n'
                                '# Part rate Link [ Link0 Link1 Link2 Link3 ]\n'
                                '0 T425a-25 1.00 0 [ HOST ... ... ... ]\n')
        loader = cls.directory / 'loader.c'
        loader.write_text(stub.read_text().replace('#include <stdlib.h>',
                                                  '#include <stdlib.h>\n#include <string.h>')
                          .replace('int WriteLink(LINK id, unsigned char *buf, unsigned int count, int timeout) { abort(); return 0; }',
                                   'static int writes;\nint WriteLink(LINK id, unsigned char *buf, unsigned int count, int timeout) { writes++; return (int) count; }')
                          + '''
char *PROGRAM_NAME = "loader-test";
int main(void)
{
    struct tpstats node;
    unsigned char code = 0;
    int ok;
    memset(&node, 0, sizeof(node));
    ok = load(1, &node, 1, 0, 0, 0, 4, &code);
    return ok && writes == 3 ? 0 : 1;
}
''')
        subprocess.run(['gcc', '-std=c90', '-pedantic-errors', '-I' + str(ROOT),
                        str(ROOT / 'cklib.c'), str(loader),
                        '-o', str(cls.directory / 'loader')], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_host(self, mode, arguments, input=''):
        return subprocess.run([str(self.directory / mode), *arguments],
                              input=input, capture_output=True, text=True, timeout=3)

    def test_native_default_and_ignore_do_not_wait_for_stdin(self):
        for arguments in ([], ['-x', '-p'], ['-x', '-f', 'missing-template']):
            with self.subTest(arguments=arguments):
                process = subprocess.Popen([str(self.directory / 'native'), *arguments],
                                           stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                           stderr=subprocess.PIPE, text=True)
                try:
                    # Keep stdin open with no data, as at an interactive terminal.
                    self.assertEqual(process.wait(timeout=3), 0)
                    self.assertIn('MOCK OPEN TMB', process.stdout.read())
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
                    process.stdin.close()
                    process.stdout.close()
                    process.stderr.close()

    def test_native_named_and_redirected_templates(self):
        for arguments, data in [(['-f', str(self.template)], ''),
                                (['-p'], self.template.read_text())]:
            result = self.run_host('native', arguments, data)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('MOCK OPEN template-link', result.stdout)

    def test_unix_keeps_automatic_redirected_input(self):
        result = self.run_host('unix', [], self.template.read_text())
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('MOCK OPEN template-link', result.stdout)

    def test_bad_template_arguments_fail_before_opening_link(self):
        for arguments in (['-f'], ['-f', 'missing-template'],
                          ['-p', '-f', str(self.template)]):
            with self.subTest(arguments=arguments):
                result = self.run_host('native', arguments)
                self.assertEqual(result.returncode, 1)
                self.assertNotIn('MOCK OPEN', result.stdout)

    def test_loading_does_not_pollute_network_output(self):
        result = self.run_host('loader', [])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, '')


if __name__ == '__main__':
    unittest.main()
