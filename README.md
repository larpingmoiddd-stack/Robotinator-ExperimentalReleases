# Robotinator

A fast, lightweight **native C** multi-pane terminal emulator built with **GTK3** and **VTE**.

Inspired by Terminator, **Robotinator** automatically reads \`/etc/shells\` and lets you spawn **different shells in every split pane on the fly**, no profile setup required.

## Quick Install

**Debian / Ubuntu / Linux Mint**
\`\`\`sh
curl -fLO https://github.com/larpingmoiddd-stack/Robotinator/releases/latest/download/robotinator_0.5.0_amd64.deb
sudo apt install ./robotinator_0.5.0_amd64.deb
\`\`\`

**Fedora / RHEL / openSUSE**
\`\`\`sh
curl -fLO https://github.com/larpingmoiddd-stack/Robotinator/releases/latest/download/robotinator-0.5.0-1.x86_64.rpm
sudo dnf install ./robotinator-0.5.0-1.x86_64.rpm
\`\`\`

**Arch Linux**
\`\`\`sh
curl -fLO https://github.com/larpingmoiddd-stack/Robotinator/releases/latest/download/PKGBUILD
makepkg -si
\`\`\`

**Gentoo**
\`\`\`sh
curl -fLO https://github.com/larpingmoiddd-stack/Robotinator/releases/latest/download/Robotinator-0.5.0.ebuild
\`\`\`

## Build from Source

\`\`\`sh
sudo apt install -y build-essential pkg-config libgtk-3-dev libvte-2.91-dev
make
sudo make install
\`\`\`

## Shell Requirements

Only shells listed in \`/etc/shells\` appear in the per-pane switcher dropdown. Shells installed via \`apt\` are added automatically, while custom binaries (such as Cargo or Homebrew builds) can be registered with:

\`\`\`sh
command -v <shell-name> | sudo tee -a /etc/shells
\`\`\`
