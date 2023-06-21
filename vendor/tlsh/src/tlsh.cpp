/*
 * TLSH is provided for use under two licenses: Apache OR BSD.
 * Users may opt to use either license depending on the license
 * restictions of the systems with which they plan to integrate
 * the TLSH code.
 */ 

/* ==============
 * Apache License
 * ==============
 * Copyright 2013 Trend Micro Incorporated
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* ===========
 * BSD License
 * ===========
 * Copyright (c) 2013, Trend Micro Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.

 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tlsh.h"
#include "tlsh_impl.h"
#include "stdio.h"
#include <errno.h>
#include <string.h>

/////////////////////////////////////////////////////
// C++ Implementation

Tlsh::Tlsh():impl(NULL)
{
    impl = new TlshImpl();
}

Tlsh::Tlsh(const Tlsh& other):impl(NULL)
{
    impl = new TlshImpl();
    *impl = *other.impl;
}

Tlsh::~Tlsh()
{
    delete impl;
}

void Tlsh::update(const unsigned char* data, unsigned int len)
{
	//
	// threaded and private options only available to
	//	windowsize == 5
	//	calling final - without calling update first
	//
	int tlsh_option	= 0;
	if (impl != NULL)
		impl->update(data, len, tlsh_option);
}

void Tlsh::final(const unsigned char* data, unsigned int len, int tlsh_option)
{
	if (NULL != impl) {
		if ((data != NULL) && (len > 0))
			impl->update(data, len, tlsh_option);
		impl->final(tlsh_option);
	}
}

const char* Tlsh::getHash(int showvers) const
{
    if ( NULL != impl )
        return impl->hash(showvers);
    else
        return "";
}

const char* Tlsh::getHash (char *buffer, unsigned int bufSize, int showvers) const
{
    if ( NULL != impl )
        return impl->hash(buffer, bufSize, showvers);
    else {
        buffer[0] = '\0';
        return buffer;
    }
}

void Tlsh::reset()
{
    if ( NULL != impl )
        impl->reset();
}

Tlsh& Tlsh::operator=(const Tlsh& other)
{
    if (this == &other) 
        return *this;

    *impl = *other.impl;
    return *this;
}

bool Tlsh::operator==(const Tlsh& other) const
{
    if( this == &other )
        return true;
    else if( NULL == impl || NULL == other.impl )
        return false;
    else
        return ( 0 == impl->compare(*other.impl) );
}

bool Tlsh::operator!=(const Tlsh& other) const 
{
    return !(*this==other);
}

int Tlsh::Lvalue()
{
	return( impl->Lvalue() );
}
int Tlsh::Q1ratio()
{
	return( impl->Q1ratio() );
}
int Tlsh::Q2ratio()
{
	return( impl->Q2ratio() );
}
int Tlsh::Checksum(int k)
{
	return( impl->Checksum(k) );
}
int Tlsh::BucketValue(int bucket)
{
	return( impl->BucketValue(bucket) );
}

int Tlsh::HistogramCount(int bucket)
{
	return( impl->HistogramCount(bucket) );
}

int Tlsh::totalDiff(const Tlsh *other, bool len_diff) const
{
    if( NULL==impl || NULL == other || NULL == other->impl )
        return -(EINVAL);
    else if ( this == other )
        return 0;
    else
        return (impl->totalDiff(*other->impl, len_diff));
}

int Tlsh::fromTlshStr(const char* str)
{
    if ( NULL == impl )
        return -(ENOMEM);
    else if ( NULL == str )
        return -(EINVAL);
    else
        return impl->fromTlshStr(str);
}

bool Tlsh::isValid() const
{
    return (impl ? impl->isValid() : false);
}
