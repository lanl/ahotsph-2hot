#!/bin/bash
echo char version_2HOT\[\] = \"`git describe --tags --dirty`\"\; > ${2}
echo char arch_2HOT\[\] = \"${1}\"\; >> ${2}
echo char compiled_date_2HOT\[\] = __DATE__\; >> ${2}
echo char compiled_time_2HOT\[\] = __TIME__\; >> ${2}
echo char compiler_2HOT\[\] = __VERSION__\; >> ${2}
