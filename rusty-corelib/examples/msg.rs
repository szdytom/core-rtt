#![no_std]
#![no_main]

use core::hint::spin_loop;

use rusty_corelib::{dev_info, entry, recv_msg, send_msg, turn};

fn assert_message_equals(buf: &[u8], expected: &[u8]) {
    assert!(buf.len() == expected.len());
    assert!(buf == expected);
}

fn run() -> ! {
    let info = dev_info();
    let my_id = info.id;
    assert!((1..=3).contains(&my_id));

    let mut last_turn = 0;
    let mut received_ok = false;
    let mut message_buf = [0_u8; 32];

    loop {
        let t = turn();
        if t == last_turn {
            spin_loop();
            continue;
        }
        last_turn = t;

        if my_id == 1 && t == 1 {
            assert!(send_msg(b"PING") == Ok(()));
            assert!(send_msg(b"PING") == Err(rusty_corelib::ErrorCode::OnCooldown));
        }

        if !received_ok && t >= 2 {
            let n = recv_msg(&mut message_buf).unwrap_or_default();
            if n > 0 {
                assert_message_equals(&message_buf[..n], b"PING");
                received_ok = true;
            }
        }

        assert!(received_ok || t < 6);

        if received_ok && t >= 6 {
            assert!(rusty_corelib::logf!("[PASS] msg ecall behavior is correct.") == Ok(()));
            loop {
                spin_loop();
            }
        }
    }
}

entry!(run);
