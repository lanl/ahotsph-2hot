#!/bin/bash
echo char Version\[\] = \"`git describe --tags --dirty`\"\; > ${2}
echo char Arch\[\] = \"${1}\"\; >> ${2}
echo char Compiled_date\[\] = __DATE__\; >> ${2}
echo char Compiled_time\[\] = __TIME__\; >> ${2}
echo char Compiler\[\] = __VERSION__\; >> ${2}
