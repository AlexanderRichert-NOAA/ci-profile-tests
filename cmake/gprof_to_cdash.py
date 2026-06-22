# gprof_to_cdash.py
#
# Parses gprof flat profile output and generates a CDash custom measurement
# XML file.
#
# Usage: python gprof_to_cdash.py <gprof_output.txt> <test_name> <output.xml>

import re
import sys

def parse_gprof(gprof_file, test_name):
    """
    Parses gprof output and yields (measurement_name, value) tuples.
    """
    # Regex to find the start of the flat profile table.
    header_re = re.compile(r'^\s*%[ \t]+cumulative[ \t]+self[ \t]+self[ \t]+total')
    # Regex to parse a data line in the flat profile.
    # Example line:
    # 33.34      0.02     0.02     7208     0.00     0.00  open
    # We capture self_ms/call (group 1) and the function name (group 2)
    line_re = re.compile(r'^\s*[\d\.]+\s+[\d\.]+\s+[\d\.]+\s+[\d\.]*\s+([\d\.]+)\s+[\d\.]+\s+([\w.<>:]+)'
    )

    in_profile_section = False
    with open(gprof_file, 'r') as f:
        for line in f:
            if not in_profile_section:
                if header_re.match(line):
                    in_profile_section = True
                continue

            # Stop if we're past the flat profile section
            if not line.strip() or line.startswith(' '):
                match = line_re.match(line)
                if not match:
                    # This could be the end of the section or a line without calls
                    continue

            match = line_re.match(line)
            if match:
                ms_per_call = match.group(1)
                func_name = match.group(2)
                measurement_name = f"{test_name}-{func_name}-self_ms_per_call"
                yield measurement_name, float(ms_per_call)

def write_cdash_xml(measurements, output_xml_file):
    """
    Writes a list of (name, value) measurements to a CDash XML file.
    """
    with open(output_xml_file, 'w') as f:
        f.write('<cdash>\n<measurements>\n')
        for name, value in measurements:
            f.write(f'  <measurement name="{name}" type="numeric/double">{value}</measurement>\n')
        f.write('</measurements>\n</cdash>\n')

if __name__ == "__main__":
    gprof_txt, test_name, output_xml = sys.argv[1:4]
    measurements = list(parse_gprof(gprof_txt, test_name))
    write_cdash_xml(measurements, output_xml)
