---
pagetitle: Package License Terms for STM32CubeF4
lang: en
header-includes: <link rel="icon" type="image/x-icon" href="_htmresc/favicon.png" />
---

::: {.row}
::: {.col-sm-12 .col-lg-3}

<center>
# These are full license terms for

# <mark>STM32CubeF4</mark>

Copyright &copy; 2023 STMicroelectronics

All rights reserved
    
[![ST logo](_htmresc/st_logo_2020.png)](https://www.st.com){.logo}
</center>
:::

::: {.col-sm-12 .col-lg-9}

## <mark>__OVERVIEW__</mark>
<div>

This Software Bill Of Material (SBOM) lists the software components of this
software package, including the copyright owner and license terms for each
component.

The full text of these licenses are below the SBOM.

__SOFTWARE BILL OF MATERIALS__

|Component                        |Copyright                                                          |License                                                                                        
|-------------------------------- |----------------------------------------------------------------   |-----------------------------------------------------------------------------------------------
|CMSIS                            |Arm Limited                                                        |[Apache-2.0](https://opensource.org/licenses/Apache-2.0) 
|CMSIS Device                     |Arm Limited - STMicroelectronics                                   |[Apache-2.0](https://opensource.org/licenses/Apache-2.0)
|STM32F4 HAL                      |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32F4-Discovery            |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32F4xx_Nucleo_144         |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32F4xx-Nucleo             |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32F401-Discovery          |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32F411E-Discovery         |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32F413H-Discovery         |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32F429I-Discovery         |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM324x9I_EVAL               |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM324xG_EVAL                |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32412G-Discovery          |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32446E_EVAL               |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32469I_EVAL               |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP STM32469I-Discovery          |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP Components                   |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|BSP Adafruit                     |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
|FreeRTOS kernel                  |Amazon.com, Inc. or its affiliates                                 |[MIT](https://opensource.org/licenses/MIT)                                                     
|FatFS                            |ChaN - STMicroelectronics                                          |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)                                   
|LibJPEG                          |Thomas G. Lane, Guido Vollbe                                       |[ANNEX 1](#collapse-section2)                                
|LwIP                             |Swedish Institute of Computer Science                              |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)                                   
|mbedTLS                          |Arm Limited                                                        |[Apache-2.0](https://opensource.org/licenses/Apache-2.0)                                       
|STM32_USB_Device_Library         |STMicroelectronics                                                 |[SLA](#collapse-section1)                                                           
|STM32_USB_Host_Library           |STMicroelectronics                                                 |[SLA](#collapse-section1)                                                          
|STemWin                          |STMicroelectronics and SEGGER Microcontroller GmbH & Co. KG        |[SLA](#collapse-section1)                                                         
|STM32_Audio                      |STMicroelectronics                                                 |[ANNEX 2](#collapse-section3)                                                         
|TouchGFX                         |STMicroelectronics                                                 |[SLA](#collapse-section1)                                                          
|STM32 Projects                   |STMicroelectronics                                                 |[SLA](#collapse-section1)([BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause) for basic Examples)  
|STM32 Utilities                  |STMicroelectronics                                                 |[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

__Notes:__ If the license is an open source license, then you can access the
terms at [www.opensource.org](https://opensource.org/). Otherwise, the full
license terms are below. If a component is not listed in the SBOM, then the SLA
shall apply unless other terms are clearly stated in the package.

</div>

::: {.collapse}                                                                                      
<input type="checkbox" id="collapse-section1" checked aria-hidden="true">                            
<label for="collapse-section1" aria-hidden="true">__SLA – Software License Agreement__</label>                                
<div>                                                                                                
                                                                                                     
SLA0048 Rev4/March 2018                                                                              
                                                                                                     
## Software license agreement                                                                        
                                                                                                     
### __SOFTWARE PACKAGE LICENSE AGREEMENT__                                                           
                                                                                                     
BY INSTALLING COPYING, DOWNLOADING, ACCESSING OR OTHERWISE USING THIS SOFTWARE PACKAGE OR ANY        
PART THEREOF (AND THE RELATED DOCUMENTATION) FROM STMICROELECTRONICS INTERNATIONAL N.V, SWISS        
BRANCH AND/OR ITS AFFILIATED COMPANIES (STMICROELECTRONICS), THE RECIPIENT, ON BEHALF OF HIMSELF     
OR HERSELF, OR ON BEHALF OF ANY ENTITY BY WHICH SUCH RECIPIENT IS EMPLOYED AND/OR ENGAGED            
AGREES TO BE BOUND BY THIS SOFTWARE PACKAGE LICENSE AGREEMENT.                                       
                                                                                                     
Under STMicroelectronics’ intellectual property rights and subject to applicable licensing terms for any third-party software
incorporated in this software package and applicable Open Source Terms (as defined here below), the redistribution,
reproduction and use in source and binary forms of the software package or any part thereof, with or without modification, are
permitted provided that the following conditions are met:                                            
                                                                                                     
1. Redistribution of source code (modified or not) must retain any copyright notice, this list of conditions and the following
disclaimer.                                                                                          
                                                                                                     
2. Redistributions in binary form, except as embedded into microcontroller or microprocessor device manufactured by or for
STMicroelectronics or a software update for such device, must reproduce the above copyright notice, this list of conditions
and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of STMicroelectronics nor the names of other contributors to this software package may be used to
endorse or promote products derived from this software package or part thereof without specific written permission.

4. This software package or any part thereof, including modifications and/or derivative works of this software package, must
be used and execute solely and exclusively on or in combination with a microcontroller or a microprocessor devices
manufactured by or for STMicroelectronics.

5. No use, reproduction or redistribution of this software package partially or totally may be done in any manner that would
subject this software package to any Open Source Terms (as defined below).

6. Some portion of the software package may contain software subject to Open Source Terms (as defined below) applicable
for each such portion (“Open Source Software”), as further specified in the software package. Such Open Source Software
is supplied under the applicable Open Source Terms and is not subject to the terms and conditions of license hereunder.
“Open Source Terms” shall mean any open source license which requires as part of distribution of software that the source
code of such software is distributed therewith or otherwise made available, or open source license that substantially
complies with the Open Source definition specified at www.opensource.org and any other comparable open source license
such as for example GNU General Public License (GPL), Eclipse Public License (EPL), Apache Software License, BSD
license and MIT license.

7. This software package may also include third party software as expressly specified in the software package subject to
specific license terms from such third parties. Such third party software is supplied under such specific license terms and is
not subject to the terms and conditions of license hereunder. By installing copying, downloading, accessing or otherwise
using this software package, the recipient agrees to be bound by such license terms with regard to such third party
software.

8. STMicroelectronics has no obligation to provide any maintenance, support or updates for the software package.

9. The software package is and will remain the exclusive property of STMicroelectronics and its licensors. The recipient will
not take any action that jeopardizes STMicroelectronics and its licensors' proprietary rights or acquire any rights in the
software package, except the limited rights specified hereunder.

10. The recipient shall comply with all applicable laws and regulations affecting the use of the software package or any part
thereof including any applicable export control law or regulation.

11. Redistribution and use of this software package partially or any part thereof other than as permitted under this license is
void and will automatically terminate your rights under this license.

THIS SOFTWARE PACKAGE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY
INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE PACKAGE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

EXCEPT AS EXPRESSLY PERMITTED HEREUNDER AND SUBJECT TO THE APPLICABLE LICENSING TERMS FOR ANY
THIRD-PARTY SOFTWARE INCORPORATED IN THE SOFTWARE PACKAGE AND OPEN SOURCE TERMS AS
APPLICABLE, NO LICENSE OR OTHER RIGHTS, WHETHER EXPRESS OR IMPLIED, ARE GRANTED UNDER ANY
PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF STMICROELECTRONICS OR ANY THIRD PARTY.

</div>
:::

::: {.collapse}
<input type="checkbox" id="collapse-section2" checked aria-hidden="true">
<label for="collapse-section2" aria-hidden="true">__ANNEX 1 - LIBJPEG_LICENSE__</label>
<div>

LEGAL ISSUES
============

In plain English:

1. We don't promise that this software works. (But if you find any bugs,
please let us know!)
2. You can use this software for whatever you want. You don't have to pay us.
3. You may not pretend that you wrote this software. If you use it in a
program, you must acknowledge somewhere in your documentation that
you've used the IJG code.

In legalese:

The authors make NO WARRANTY or representation, either express or implied,
with respect to this software, its quality, accuracy, merchantability, or
fitness for a particular purpose. This software is provided "AS IS", and you,
its user, assume the entire risk as to its quality and accuracy.

This software is copyright (C) 1991-1998, Thomas G. Lane.
All Rights Reserved except as specified below.

Permission is hereby granted to use, copy, modify, and distribute this
software (or portions thereof) for any purpose, without fee, subject to these
conditions:
(1) If any part of the source code for this software is distributed, then this
README file must be included, with this copyright and no-warranty notice
unaltered; and any additions, deletions, or changes to the original files
must be clearly indicated in accompanying documentation.
(2) If only executable code is distributed, then the accompanying
documentation must state that "this software is based in part on the work of
the Independent JPEG Group".
(3) Permission for use of this software is granted only if the user accepts
full responsibility for any undesirable consequences; the authors accept
NO LIABILITY for damages of any kind.

These conditions apply to any software derived from or based on the IJG code,
not just to the unmodified library. If you use our work, you ought to
acknowledge us.

Permission is NOT granted for the use of any IJG author's name or company name
in advertising or publicity relating to this software or products derived from
it. This software may be referred to only as "the Independent JPEG Group's
software".

We specifically permit and encourage the use of this software as the basis of
commercial products, provided that all warranty or liability claims are
assumed by the product vendor.


ansi2knr.c is included in this distribution by permission of L. Peter Deutsch,
sole proprietor of its copyright holder, Aladdin Enterprises of Menlo Park, CA.
ansi2knr.c is NOT covered by the above copyright and conditions, but instead
by the usual distribution terms of the Free Software Foundation; principally,
that you must include source code if you redistribute it. (See the file
ansi2knr.c for full details.) However, since ansi2knr.c is not needed as part
of any program generated from the IJG code, this does not limit you more than
the foregoing paragraphs do.

The Unix configuration script "configure" was produced with GNU Autoconf.
It is copyright by the Free Software Foundation but is freely distributable.
The same holds for its supporting scripts (config.guess, config.sub,
ltconfig, ltmain.sh). Another support script, install-sh, is copyright
by M.I.T. but is also freely distributable.

It appears that the arithmetic coding option of the JPEG spec is covered by
patents owned by IBM, AT&T, and Mitsubishi. Hence arithmetic coding cannot
legally be used without obtaining one or more licenses. For this reason,
support for arithmetic coding has been removed from the free JPEG software.
(Since arithmetic coding provides only a marginal gain over the unpatented
Huffman mode, it is unlikely that very many implementations will support it.)
So far as we are aware, there are no patent restrictions on the remaining
code.

The IJG distribution formerly included code to read and write GIF files.
To avoid entanglement with the Unisys LZW patent, GIF reading support has
been removed altogether, and the GIF writer has been simplified to produce
"uncompressed GIFs". This technique does not use the LZW algorithm; the
resulting GIF files are larger than usual, but are readable by all standard
GIF decoders. 


</div>
:::

::: {.collapse}
<input type="checkbox" id="collapse-section3" checked aria-hidden="true">
<label for="collapse-section3" aria-hidden="true">__ANNEX 2 - SLA0047__</label>
<div>

SLA0047 Rev2/July 2020

## Software license agreement

### __SOFTWARE LICENSE AGREEMENT__

This is a legally enforceable contract (Agreement) between STMicroelectronics
International N.V., Swiss branch and/or its affiliated companies
(STMicroelectronics or ST) and you, on behalf of yourself, or on behalf of the
entity by which you are employed and/or engaged (You).

Please indicate Your acceptance of this contract by selecting “I ACCEPT” or
indicate Your rejection of this contract by selecting “I DO NOT ACCEPT” as
indicated below in the media,

AND/OR

BY INSTALLING COPYING, DOWNLOADING, ACCESSING OR OTHERWISE USING THIS LICENSED
SOFTWARE (INCLUDING WITHOUT LIMITATION ANY DOCUMENTATION AND OTHER PARTS
THEREOF) YOU AGREE TO BE BOUND BY THIS SOFTWARE LICENSE AGREEMENT.

__DEFINITIONS__

Licensed Software: means the software/firmware, examples, project template and
all the related documentation and design tools licensed and delivered in the
form of object and/or source code as the case may be.

Your Product: means Your and Your end-users’ product or system, and all the
related documentation, that includes or works in combination with an executable
version of the Licensed Software or derivative of the Licensed Software as
permitted in this Agreement and provided further that such Licensed Software or
derivative of the Licensed Software as permitted in this Agreement executes
solely and exclusively on microcontroller devices manufactured by or for ST.

__LICENSE__

STMicroelectronics grants You a non-exclusive, worldwide, non-transferable
(whether by assignment or otherwise unless expressly authorized by ST) non
sub-licensable, revocable, royalty-free limited license of the Licensed Software
to:

(i) use, make copies and prepare derivative works of Licensed Software that is
delivered by ST in source code format for the sole purpose of designing,
developing and manufacturing Your Products;

(ii) use, make copies and prepare derivative works (without reverse engineering)
of the Licensed Software that is delivered by ST in object code format for the
sole purpose of designing, developing and manufacturing Your Products;

(iii) make copies and prepare derivative works of the documentation part of the
Licensed Software for the sole purpose of providing documentation for Your
Products and their usage;

(iv) make, have made, import, export and otherwise distribute the Licensed
Software or derivative of the Licensed Software as permitted in this Agreement
solely in object code format, as incorporated in Your Products, or for execution
on Your Products, provided such distribution is subject to license terms as
protective of the Licensed Software and ST’s rights in the Licensed Software as
this Agreement (including without limitation a warranty disclaimer and a
limitation of liability as protective of ST as those contained in this
Agreement).

__OWNERSHIP AND COPYRIGHT__

Title to the Licensed Software, related documentation and all copies thereof
remain with ST and/or its licensors. You may not remove any copyright notice,
warranty disclaimer, or other proprietary notice from the Licensed Software or
from any permitted copies of the Licensed Software. You shall prevent any
unauthorized copying of the Licensed Software, including without limitation any
documentation parts.

__RESTRICTIONS__

Unless otherwise explicitly stated in this Agreement, You may not sell, assign,
sublicense, lease, rent or otherwise distribute the Licensed Software for
commercial purposes, in whole or in part.

You acknowledge and agree that any use, adaptation translation or transcription
of the Licensed Software or any portion or derivative thereof, for use with
processors manufactured by or for any entity other than ST is a material breach
of this Agreement.

You shall not decompile the object code of the Licensed Software or otherwise
reverse engineer the Licensed Software. You shall not use the Licensed Software
as a basis to create similar or competing software or products.

You shall comply with all applicable laws and regulations affecting the use of
the Licensed Software including any applicable export control law or regulation.

Neither the name nor any trademark of STMicroelectronics nor other contributors
to the Licensed Software may be used to endorse or promote software or products
derived from the Licensed Software (including any part thereof) without specific
written permission.

No use, reproduction or redistribution of this Licensed Software partially or
totally may be done in any manner that would subject this Licensed Software to
any Open Source Terms (as defined below).

__OPEN SOURCE SOFTWARE__

The Licensed Software may contain software subject to Open Source Terms (as
defined below) applicable for each such portion (Open Source Software), as
further specified in the Licensed Software. Such Open Source Software is
supplied under the applicable Open Source Terms and is not subject to the terms
and conditions of license hereunder. By installing copying, downloading,
accessing or otherwise using the Licensed Software, You agree to be bound by
such Open Source Terms with regard to such Open Source Software. Open Source
Terms: mean any open source license which requires as part of distribution of
software that the source code of such software is distributed therewith or
otherwise made available, or open source license that substantially complies
with the Open Source definition specified at www.opensource.org and any other
comparable open source license such as for example GNU General Public License
(GPL), Eclipse Public License (EPL), Apache Software License, BSD license and
MIT license.

__THIRD PARTY SOFTWARE__

Certain portions of the Licensed Software may be subject to third party license
terms as expressly specified in the Licensed Software. In such event, those
portions are supplied under the specified third party license terms and are not
subject to the license terms of this Agreement. By installing copying,
downloading, accessing or otherwise using the Licensed Software, You agree to be
bound by such third party license terms with regard to those portions.

__NO OTHER RIGHTS OR LICENSES__

No rights or licenses are granted to You, except as expressly stated in this
Agreement. Without limiting the generality of the foregoing, no rights or
licenses are granted in any products, technology or intellectual property rights
of ST except those embodied in the Licensed Software.

__NO WARRANTY__

The Licensed Software is provided “as is” and “with all faults” without warranty
of any kind expressed or implied. ST and its licensors expressly disclaim all
warranties, expressed, implied or otherwise, including without limitation,
warranties of merchantability, fitness for a particular purpose and
non-infringement of intellectual property rights. ST does not warrant that the
use in whole or in part of the Licensed Software will be interrupted or error
free, will meet Your requirements, or will operate with the combination of
hardware and software selected by You.

You are responsible for determining whether the Licensed Software will be
suitable for your intended use or application or will achieve your intended
results. ST will not be liable to You and/or to any third party for the
derivative works of the Licensed Software developed by You.

ST has not authorised anyone to make any representation or warranty for the
Licensed Software, and any technical, applications or design information or
advice, quality characterization, reliability data or other services provided by
ST shall not constitute any representation or warranty by ST or alter this
disclaimer or warranty, and no additional obligations or liabilities beyond
those explicitly stated in this Agreement shall arise from ST’s providing such
information or services. ST does not assume or authorize any other person to
assume for it any other liability in connection with the Licensed Software.

Nothing contained in this Agreement will be construed as:

(i) a warranty or representation by ST to maintain production of any ST device
or other hardware or software with which the Licensed Software may be used or to
otherwise maintain or support the Licensed Software in any manner; or

(ii) a commitment from ST and/or its licensors to bring or prosecute actions or
suits against third parties for infringement of any of the rights licensed
hereby, or conferring any rights to bring or prosecute actions or suits against
third parties for infringement. However, ST has the right to terminate this
Agreement immediately upon receiving notice of any claim, suit or proceeding
that alleges that the Licensed Software or Your use or distribution of the
Licensed Software infringes any third party intellectual property rights.

All other warranties, conditions or other terms implied by law are excluded to
the fullest extent permitted by law.

__LIMITATION OF LIABILITIES__

In no event shall ST or its licensors be liable to You or any third party for
any indirect, special, consequential, incidental, punitive damages or other
damages (including but not limited to, the cost of labor, re-qualification,
delay, loss of profits, loss of revenues, loss of data, costs of procurement of
substitute goods or services or the like) whether based on contract, tort, or
any other legal theory, relating to or in connection with the Licensed Software,
the documentation or this Agreement, even if ST has been advised of the
possibility of such damages.

In no event shall ST’s liability to You or any third party under this Agreement,
including any claim with respect of any third party intellectual property
rights, for any cause of action exceed 100 US$. This section does not apply to
the extent prohibited by law. For the purposes of this section, any liability of
ST shall be treated in the aggregate.

__TERMINATION__

ST may terminate this Agreement, including its licenses, at any time if You are
in material breach of any of its terms and conditions and You have failed to
cure such breach within 30 (thirty) days after receiving notification from ST of
such breach. Upon termination, You will immediately destroy or return all copies
of the Licensed Software (including without limitation any documentation) to ST.
After termination, Your sole surviving right will be to continue to use the
Licensed Software solely to the extent that it was already incorporated before
Your material breach of this Agreement into Your Products, which were sold by
You before the date of Your material breach.

__APPLICABLE LAW AND JURISDICTION__

This Agreement and matters connected with the validity, interpretation or
performance thereof shall be governed, construed, interpreted, and applied in
all respects by the law in place in Switzerland, without resort to conflict of
laws principles. The Parties expressly agree that the United Nations Convention
on Contracts for the International Sale of Goods shall not apply to this
Agreement or to their relationship.

Dispute Resolution. The Parties consent to exclusive jurisdiction in the court
of Canton of Geneva, Switzerland for purposes of any litigation arising from
this Agreement. For the avoidance of doubt, nothing in this section shall
prevent either Party seeking interim injunctive relief from a court or tribunal
of appropriate jurisdiction.

__SEVERABILITY__

If any provision of this agreement is or becomes, at any time or for any reason,
unenforceable or invalid, no other provision of this agreement shall be affected
thereby, and the remaining provisions of this agreement shall continue with the
same force and effect as if such unenforceable or invalid provisions had not
been inserted in this Agreement.

__WAIVER__

The failure or delay of either party to enforce any provision of this Agreement
shall not operate or be construed as a waiver of any other or a subsequent
breach of the same or a different provision.

__ASSIGNMENT__

This Agreement may not be assigned by You, nor any of Your rights or obligations
hereunder, to any third party without prior written consent of ST. In the event
that this Agreement is assigned effectively to a third party, this Agreement
shall bind upon successors and assigns of the parties hereto.

__RELATIONSHIP OF THE PARTIES__

Nothing in this Agreement shall create, or be deemed to create, a partnership or
the relationship of principal and agent or employer and employee between the
Parties. Neither Party has the authority or power to bind, to contract in the
name of or to create a liability for the other in any way or for any purpose.

</div>
:::

:::
:::