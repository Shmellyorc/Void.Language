"""Check the cached workflow's explicitly declared platform selections."""

import json
import re


def assert_cached_qualification_matrix(test, workflow):
    matrix_os = re.search(r'^        os: \$\{\{ fromJSON\((.+)\) \}\}$', workflow, re.MULTILINE)
    test.assertIsNotNone(matrix_os)
    linux, windows, macos, default = matrix_os[1].split(' || ')
    for scope, expression, runner in (
        ('linux', linux, 'ubuntu-24.04'), ('windows', windows, 'windows-2022'),
        ('macos', macos, 'macos-latest'),
    ):
        test.assertEqual(expression, f"inputs.platform == '{scope}' && '{json.dumps([runner])}'")

    configurations = re.search(r'^        configuration: \[([^\]]+)\]$', workflow, re.MULTILINE)
    test.assertIsNotNone(configurations)
    excluded = set(re.findall(r'^          - \{os: ([\w.-]+), configuration: (\w+)\}$', workflow, re.MULTILINE))
    full_matrix = [(runner, profile) for runner in json.loads(default[1:-1])
                   for profile in configurations[1].split(', ') if (runner, profile) not in excluded]
    test.assertCountEqual(full_matrix, [
        ('ubuntu-24.04', 'Debug'), ('ubuntu-24.04', 'Release'), ('ubuntu-24.04', 'Sanitizers'),
        ('windows-2022', 'Debug'), ('windows-2022', 'Release'),
        ('macos-latest', 'Debug'), ('macos-latest', 'Release'),
    ])
