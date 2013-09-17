#!/bin/bash

# // @copyright
# //
# // =========================================================================
# // Copyright 2012 WizziLab
# //
# // Licensed under the Apache License, Version 2.0 (the "License");
# // you may not use this file except in compliance with the License.
# // You may obtain a copy of the License at
# //
# // http://www.apache.org/licenses/LICENSE-2.0
# //
# // Unless required by applicable law or agreed to in writing, software
# // distributed under the License is distributed on an "AS IS" BASIS,
# // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# // See the License for the specific language governing permissions and
# // limitations under the License.
# // =========================================================================
# //
# // @endcopyright
# //
# // @file           otc_version.sh
# // @brief          OTCOM tool version generation script
# //
# // =========================================================================

OUTPUT_FILE="otc_version.h"

# try to get date from git
VERSION=`git log -1 --date=short | grep "Date" | sed 's/[a-z,A-Z,:, ,-]//g'`
VERSION=`echo ${VERSION:3:1}`.`echo ${VERSION:4:4}`
BRANCH=`git branch | grep "*" | sed 's/[*, ,]//g'`

# if source is not a git repository, use date
if [ "$VERSION" == "" ]
then
    VERSION=`date +%y`
    VERSION=`echo ${VERSION:1}`
    VERSION=$VERSION.`date +%0m%0e`
    BRANCH="used compile date"
fi

# if branch is not master, add branch information
if [ "$BRANCH" != "master" ]
then
    VERSION=$VERSION" ("$BRANCH")"
fi

echo "/// @copyright"                                                                 > $OUTPUT_FILE
echo "///"                                                                           >> $OUTPUT_FILE
echo "/// =========================================================================" >> $OUTPUT_FILE
echo "/// Copyright 2012 WizziLab"                                                   >> $OUTPUT_FILE
echo "///"                                                                           >> $OUTPUT_FILE
echo "/// Licensed under the Apache License, Version 2.0 (the "License");"           >> $OUTPUT_FILE
echo "/// you may not use this file except in compliance with the License."          >> $OUTPUT_FILE
echo "/// You may obtain a copy of the License at"                                   >> $OUTPUT_FILE
echo "///"                                                                           >> $OUTPUT_FILE
echo "/// http://www.apache.org/licenses/LICENSE-2.0"                                >> $OUTPUT_FILE
echo "///"                                                                           >> $OUTPUT_FILE
echo "/// Unless required by applicable law or agreed to in writing, software"       >> $OUTPUT_FILE
echo "/// distributed under the License is distributed on an "AS IS" BASIS,"         >> $OUTPUT_FILE
echo "/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."  >> $OUTPUT_FILE
echo "/// See the License for the specific language governing permissions and"       >> $OUTPUT_FILE
echo "/// limitations under the License."                                            >> $OUTPUT_FILE
echo "/// =========================================================================" >> $OUTPUT_FILE
echo "///"                                                                           >> $OUTPUT_FILE
echo "/// @endcopyright"                                                             >> $OUTPUT_FILE
echo "//"                                                                            >> $OUTPUT_FILE
echo "/// @file           otc_version.h"                                             >> $OUTPUT_FILE
echo "/// @brief          OTCOM tool version"                                        >> $OUTPUT_FILE
echo "//"                                                                            >> $OUTPUT_FILE
echo "// ========================================================================="  >> $OUTPUT_FILE
echo ""                                                                              >> $OUTPUT_FILE
echo "#ifndef __OTC_VERSION_H__"                                                     >> $OUTPUT_FILE
echo "#define __OTC_VERSION_H__"                                                     >> $OUTPUT_FILE
echo ""                                                                              >> $OUTPUT_FILE
echo "#define OTC_VERSION \""$VERSION"\""                                            >> $OUTPUT_FILE
echo ""                                                                              >> $OUTPUT_FILE
echo "#endif // __OTC_VERSION_H__"                                                   >> $OUTPUT_FILE
echo ""                                                                              >> $OUTPUT_FILE

