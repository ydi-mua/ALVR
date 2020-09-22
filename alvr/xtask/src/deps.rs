use crate::*;

use cached_path::*;
use std::{fs, io::ErrorKind};

fn dependencies_dir() -> PathBuf {
    workspace_dir().join("deps")
}

fn install_rust_android_gradle() -> BResult {
    static PLUGIN_COMMIT: &str = "6e553c13ef2d9bb40b58a7675b96e0757d1b0443";
    static PLUGIN_VERSION: &str = "0.8.3";

    let rust_android_archive_url = format!(
        "https://github.com/mozilla/rust-android-gradle/archive/{}.zip",
        PLUGIN_COMMIT
    );

    let download_path =
        cached_path_with_options(&rust_android_archive_url, &Options::default().extract())?;
    let download_path = download_path.join(format!("rust-android-gradle-{}", PLUGIN_COMMIT));

    #[cfg(windows)]
    let gradlew_path = download_path.join("gradlew.bat");
    #[cfg(target_os = "linux")]
    let gradlew_path = download_path.join("gradlew");

    run_with_args_in(
        &download_path,
        &gradlew_path.to_string_lossy(),
        &["publish"],
    )?;

    let dep_dir = dependencies_dir().join("rust-android-gradle");
    match fs::create_dir_all(&dep_dir) {
        Ok(_) => {}
        Err(e) => {
            if e.kind() != ErrorKind::AlreadyExists {
                return Err(Box::new(e));
            }
        }
    }

    // Workaround for long path issue on Windows - canonicalize
    let plugin_path = download_path.canonicalize()?;
    let plugin_path = plugin_path
        .join("samples")
        .join("maven-repo")
        .join("org")
        .join("mozilla")
        .join("rust-android-gradle")
        .join("rust-android")
        .join(PLUGIN_VERSION)
        .join(format!("rust-android-{}.jar", PLUGIN_VERSION));
    fs::copy(
        plugin_path,
        dep_dir.join(format!("rust-android-{}.jar", PLUGIN_VERSION)),
    )?;

    Ok(())
}

pub fn install_deps() -> BResult {
    install_rust_android_gradle()?;

    Ok(())
}
