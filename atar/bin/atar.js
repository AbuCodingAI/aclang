#!/usr/bin/env node

const path = require('path');
const fs = require('fs');
const { execSync } = require('child_process');
const Atar = require('../lib/atar');

const command = process.argv[2];
const arg = process.argv[3];

const atar = new Atar(process.cwd());

async function main() {
  try {
    switch (command) {
      case 'assess':
        await atar.assess();
        break;

      case 'package':
        await atar.package(arg);
        break;

      case 'get':
        await atar.get(arg);
        break;

      case 'publish':
        await atar.publish(arg);
        break;

      case 'install':
        await atar.install(arg);
        break;

      case 'ready':
        await atar.ready();
        break;

      case '--help':
      case '-h':
      case 'help':
        showHelp();
        break;

      case '--version':
      case '-v':
        console.log('atar 1.0.0');
        break;

      default:
        console.log(`Unknown command: ${command}`);
        showHelp();
        process.exit(1);
    }
  } catch (err) {
    console.error(`❌ Error: ${err.message}`);
    process.exit(1);
  }
}

function showHelp() {
  console.log(`
atar - AC Package Manager

Usage:
  atar assess              Analyze current directory (what's good, what's missing)
  atar package [name]      Create .tar file from package
  atar get [package]       Show atar.yaml details
  atar ready               Auto-generate atar.yaml from LICENSE
  atar publish [file.tar]  Publish to GitHub releases
  atar install [user/pkg]  Install from GitHub
  atar --help              Show this help
  atar --version           Show version

Examples:
  atar assess              # Check if ready to package
  atar ready               # Create atar.yaml + my_lib.tar
  atar package my_lib      # Create my_lib.tar
  atar get my_lib          # Show atar.yaml for my_lib
  atar publish my_lib.tar  # Upload to GitHub
  atar install you/my_lib  # Download from GitHub
`);
}

main().catch(err => {
  console.error(err);
  process.exit(1);
});
