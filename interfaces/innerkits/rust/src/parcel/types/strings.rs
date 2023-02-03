/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

use super::*;
use crate::{
    ipc_binding, BorrowedMsgParcel, Result, result_status,
    AsRawPtr
};
use std::convert::TryInto;
use std::mem::MaybeUninit;

impl SerOption for str {}
impl SerOption for String {}
impl DeOption for String {}

impl Serialize for str {
    fn serialize(&self, parcel: &mut BorrowedMsgParcel<'_>) -> Result<()> {
        let ret = unsafe {
            ipc_binding::CParcelWriteString(
                parcel.as_mut_raw(),
                self.as_ptr() as *const c_char,
                self.as_bytes().len().try_into().unwrap()
            )};
        result_status::<()>(ret, ())
    }
}

impl Serialize for String {
    fn serialize(&self, parcel: &mut BorrowedMsgParcel<'_>) -> Result<()> {
        self.as_str().serialize(parcel)
    }
}

impl Deserialize for String {
    fn deserialize(parcel: &BorrowedMsgParcel<'_>) -> Result<Self> {
        let mut vec: Option<Vec<u8>> = None;
        let ok_status = unsafe {
            // SAFETY: `parcel` always contains a valid pointer to a  `CParcel`
            ipc_binding::CParcelReadString(
                parcel.as_raw(),
                &mut vec as *mut _ as *mut c_void,
                allocate_vec_with_buffer::<u8>
            )
        };

        if ok_status {
            vec_to_string(vec)
        } else {
            Err(-1)
        }
    }
}

impl SerArray for &str {
    fn ser_array(slice: &[Self], parcel: &mut BorrowedMsgParcel<'_>) -> Result<()> {
        let ret = unsafe {
            // SAFETY:
            ipc_binding::CParcelWriteStringArray(
                parcel.as_mut_raw(),
                slice.as_ptr() as *const c_void,
                slice.len().try_into().unwrap(),
                on_str_writer,
            )
        };
        result_status::<()>(ret, ())
    }
}

impl SerArray for String {
    fn ser_array(slice: &[Self], parcel: &mut BorrowedMsgParcel<'_>) -> Result<()> {
        let ret = unsafe {
            // SAFETY:
            ipc_binding::CParcelWriteStringArray(
                parcel.as_mut_raw(),
                slice.as_ptr() as *const c_void,
                slice.len().try_into().unwrap(),
                on_string_writer,
            )
        };
        println!("write String array result: {}", ret);
        result_status::<()>(ret, ())
    }
}

impl DeArray for String {
    fn de_array(parcel: &BorrowedMsgParcel<'_>) -> Result<Option<Vec<Self>>> {
        let mut vec: Option<Vec<MaybeUninit<Self>>> = None;
        let ok_status = unsafe {
            // SAFETY: `parcel` always contains a valid pointer to a  `CParcel`
            // `allocate_vec<T>` expects the opaque pointer to
            // be of type `*mut Option<Vec<MaybeUninit<T>>>`, so `&mut vec` is
            // correct for it.
            ipc_binding::CParcelReadStringArray(
                parcel.as_raw(),
                &mut vec as *mut _ as *mut c_void,
                on_string_reader,
            )
        };
        if ok_status {
            let vec: Option<Vec<Self>> = unsafe {
                // SAFETY: all the MaybeUninits are now properly
                // initialized.
                vec.map(|vec| vec_assume_init(vec))
            };
            Ok(vec)
        } else {
            println!("read string from native fail");
            Err(-1)
        }
    }
}

/// Callback to serialize a String array to c++ std::vector<std::string>.
///
/// Safety: We are relying on c interface to not overrun our slice. As long
/// as it doesn't provide an index larger than the length of the original
/// slice in ser_array, this operation is safe. The index provided
/// is zero-based.
#[allow(dead_code)]
unsafe extern "C" fn on_str_writer(
    array: *const c_void, // C++ vector pointer
    value: *mut c_void, // Rust slice pointer
    len: u32,
) -> bool {
    if len == 0 {
        return false;
    }
    let len = len as usize;
    let slice: &[&str] = std::slice::from_raw_parts(value.cast(), len);

    for item in slice.iter().take(len) {
        let ret = unsafe {
            ipc_binding::CParcelWriteStringElement(array,
                item.as_ptr() as *const c_char,
                item.as_bytes().len().try_into().unwrap())
        };
        if !ret {
            return false;
        }
    }
    true
}

/// Callback to serialize a String array to c++ std::vector<std::string>.
///
/// Safety: We are relying on c interface to not overrun our slice. As long
/// as it doesn't provide an index larger than the length of the original
/// slice in ser_array, this operation is safe. The index provided
/// is zero-based.
#[allow(dead_code)]
unsafe extern "C" fn on_string_writer(
    array: *const c_void, // C++ vector pointer
    value: *mut c_void, // Rust slice pointer
    len: u32,
) -> bool {
    if len == 0 {
        return false;
    }
    let len = len as usize;
    let slice: &[String] = std::slice::from_raw_parts(value.cast(), len);

    for item in slice.iter().take(len) {
        let ret = unsafe {
            ipc_binding::CParcelWriteStringElement(array,
                item.as_ptr() as *const c_char,
                item.as_bytes().len().try_into().unwrap())
        };
        if !ret {
            return false;
        }
    }
    true
}

/// Callback to deserialize a String element in Vector<String>.
///
/// The opaque array data pointer must be a mutable pointer to an
/// `Option<Vec<MaybeUninit<T>>>` with at least enough elements for `index` to be valid
/// (zero-based).
#[allow(dead_code)]
unsafe extern "C" fn on_string_reader(
    data: *const c_void, // C++ vector pointer
    value: *mut c_void, // Rust vector pointer
    len: u32, // C++ vector length
) -> bool {
    // Allocate Vec<String> capacity, data_len will set correctly by vec.push().
    unsafe { allocate_vec_maybeuninit::<String>(value, 0) };
    let vec = &mut *(value as *mut Option<Vec<MaybeUninit<String>>>);
    for index in 0..len {
        let mut vec_u8: Option<Vec<u8>> = None;
        let ok_status = unsafe {
            ipc_binding::CParcelReadStringElement(
                index,
                data,
                &mut vec_u8 as *mut _ as *mut c_void,
                allocate_vec_with_buffer::<u8>
            )
        };
        if ok_status {
            if let Ok(string) = vec_to_string(vec_u8) {
                if let Some(new_vec) = vec {
                    new_vec.push(MaybeUninit::new(string));
                } else {
                    println!("on_string_reader allocate vec failed");
                    return false;
                }
            } else {
                println!("on_string_reader vec_to_string failed");
                return false;
            }
        } else {
            return false;
        }
    }
    true
}

fn vec_to_string(vec: Option<Vec<u8>>) -> Result<String> {
    let value = vec.map(|s| {
        // The vector includes a null-terminator and
        // we don't want the string to be null-terminated for Rust.
        String::from_utf8(s).or(Err(-1))
    });
    if let Some(ret) = value {
        ret
    } else {
        println!("convert vector u8 to String fail");
        Err(-1)
    }
}