/******************************************************************************
 *
 *  Copyright 2022 Google LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include <stdio.h>

int check_ltpf(void);
int check_mdct(void);

int main()
{
    int r, ret = 0;

    printf("Checking LTPF Neon... "); fflush(stdout);
    printf("%s\n", (r = check_ltpf()) == 0 ? "OK" : "Failed");
    ret = ret || r;

    printf("Checking MDCT Neon... "); fflush(stdout);
    printf("%s\n", (r = check_mdct()) == 0 ? "OK" : "Failed");
    ret = ret || r;

    return ret;
}
