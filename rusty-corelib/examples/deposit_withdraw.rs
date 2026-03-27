#![no_std]
#![no_main]

use core::hint::spin_loop;

use rusty_corelib::{ErrorCode, deposit, dev_info, entry, turn, withdraw};

fn run() -> ! {
    if dev_info().id != 1 {
        loop {
            spin_loop();
        }
    }

    let mut last_turn = 0;
    let mut phase = 0;

    loop {
        let t = turn();
        if t == last_turn {
            spin_loop();
            continue;
        }
        last_turn = t;

        let energy_before = dev_info().energy;

        if phase == 0 {
            if t >= 9 {
                phase = 1;
            }
            continue;
        }

        if phase == 1 {
            assert!(energy_before >= 5);
            assert!(deposit(5) == Ok(()));
            assert!(dev_info().energy == energy_before - 5);
            phase = 2;
            continue;
        }

        if phase == 2 {
            assert!(withdraw(10) == Err(ErrorCode::InsufficientEnergy));
            assert!(withdraw(3) == Ok(()));
            assert!(dev_info().energy == energy_before + 3);
            phase = 3;
            continue;
        }

        if phase == 3 {
            assert!(deposit(0) == Err(ErrorCode::OutOfRange));
            assert!(deposit(3) == Ok(()));
            assert!(dev_info().energy == energy_before - 3);
            phase = 4;
            continue;
        }

        if phase == 4 {
            assert!(deposit(1) == Ok(()));
            assert!(deposit(1) == Err(ErrorCode::OnCooldown));
            assert!(withdraw(1) == Err(ErrorCode::OnCooldown));
            assert!(dev_info().energy == energy_before - 1);
            phase = 5;
            continue;
        }

        if phase == 5 {
            assert!(
                rusty_corelib::logf!("[PASS] deposit/withdraw ecall behavior is correct.")
                    == Ok(())
            );
            loop {
                spin_loop();
            }
        }
    }
}

entry!(run);
