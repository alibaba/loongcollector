#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import sys

ignored_chars = '#\n \t:/**/'

license_header = ' '.join(
    [
        line.strip(ignored_chars) for line in """
        #
        # Licensed to the Apache Software Foundation (ASF) under one or more
        # contributor license agreements.  See the NOTICE file distributed with
        # this work for additional information regarding copyright ownership.
        # The ASF licenses this file to You under the Apache License, Version 2.0
        # (the "License"); you may not use this file except in compliance with
        # the License.  You may obtain a copy of the License at
        #
        #     http://www.apache.org/licenses/LICENSE-2.0
        #
        # Unless required by applicable law or agreed to in writing, software
        # distributed under the License is distributed on an "AS IS" BASIS,
        # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
        # See the License for the specific language governing permissions and
        # limitations under the License.
        #
        """.splitlines()
    ]
).strip(ignored_chars)

comment_leading_chars = ('#', '::', '/*', '*', ' ')


def walk_through_dir(d) -> bool:
    checked = True
    for root, sub_dirs, files in os.walk(d):
        for filename in files:
            file_path = os.path.join(root, filename)
            if '.git' in file_path or filename.endswith('.md'):
                continue
            with open(file_path, 'r') as f:
                header = ' '.join([
                    line.strip(ignored_chars) for line in f.readlines() if line.startswith(comment_leading_chars)
                ]).strip()
                print('%s license header in file: %s' % ('✅' if license_header in header else '❌', file_path))
                checked &= license_header in header
    return checked


if __name__ == "__main__":
    checked = True
    for _, directory in enumerate(sys.argv):
        checked &= walk_through_dir(directory)
    if not checked:
        sys.exit(1)
