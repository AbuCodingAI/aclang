#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const testDir = path.join(__dirname, '../test-package');

console.log('\n🧪 Testing atar...\n');

// Setup test directory
if (fs.existsSync(testDir)) {
  execSync(`rm -rf "${testDir}"`);
}
fs.mkdirSync(testDir, { recursive: true });

// Create test files
fs.writeFileSync(path.join(testDir, 'lib.ac'), `AC->LIB

Make hello func(name)
    return $Hello, $ + name

Make reverse func(s)
    return s
`);

fs.writeFileSync(path.join(testDir, 'LICENSE'), `MIT License

Copyright (c) 2026 Test Author

Permission is hereby granted, free of charge...
`);

fs.writeFileSync(path.join(testDir, 'README.md'), `# Test Package

This is a test AC library.
`);

// Initialize git repo
execSync('git init', { cwd: testDir, stdio: 'pipe' });
execSync('git config user.name "Test User"', { cwd: testDir, stdio: 'pipe' });
execSync('git config user.email "test@example.com"', { cwd: testDir, stdio: 'pipe' });
execSync('git remote add origin https://github.com/test/test-lib.git', { cwd: testDir, stdio: 'pipe' });

const Atar = require('../lib/atar');
const atar = new Atar(testDir);

async function runTests() {
  try {
    // Test 1: assess
    console.log('Test 1: atar assess');
    await atar.assess();

    // Test 2: ready (should create atar.yaml)
    console.log('Test 2: atar ready');
    await atar.ready();

    // Test 3: check if atar.yaml was created
    if (fs.existsSync(path.join(testDir, 'atar.yaml'))) {
      console.log('✅ atar.yaml created\n');
    } else {
      throw new Error('atar.yaml not created');
    }

    // Test 4: get
    console.log('Test 4: atar get');
    await atar.get();

    // Test 5: check if .tar was created
    const tarExists = fs.existsSync(path.join(testDir, 'test-package.tar'));
    if (tarExists) {
      console.log('✅ test-package.tar created\n');
    } else {
      throw new Error('tar file not created');
    }

    // Test 6: package (explicit)
    console.log('Test 6: atar package test-lib');
    await atar.package('test-lib');

    // Cleanup
    execSync(`rm -rf "${testDir}"`);

    console.log('\n✅ All tests passed!\n');
  } catch (err) {
    console.error(`\n❌ Test failed: ${err.message}\n`);
    process.exit(1);
  }
}

runTests();
