const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

class Atar {
  constructor(cwd) {
    this.cwd = cwd;
    this.atarYamlPath = path.join(cwd, 'atar.yaml');
    this.licensePath = path.join(cwd, 'LICENSE');
  }

  // atar assess - Analyze directory
  async assess() {
    console.log('\n📋 Assessing package...\n');

    const checks = {
      'atar.yaml': fs.existsSync(this.atarYamlPath),
      'LICENSE': fs.existsSync(this.licensePath),
      'lib.ac': fs.existsSync(path.join(this.cwd, 'lib.ac')),
      '.tar file': this.findTarFile(),
      'git repo': this.isGitRepo(),
      'git remote': this.hasGitRemote()
    };

    let allGood = true;
    for (const [check, exists] of Object.entries(checks)) {
      const icon = exists ? '✅' : '❌';
      console.log(`${icon} ${check}`);
      if (!exists && check !== '.tar file') allGood = false;
    }

    console.log('\n' + (allGood ? '✅ Ready to package!' : '⚠️ Missing files. Run: atar ready'));
    console.log('');
  }

  // atar package - Create .tar file
  async package(name) {
    if (!name) {
      const yaml = this.loadYaml();
      if (!yaml || !yaml.name) {
        throw new Error('No atar.yaml found. Run: atar ready');
      }
      name = yaml.name;
    }

    console.log(`\n📦 Packaging ${name}...\n`);

    // Find files to include
    const includeFiles = [
      'lib.ac',
      'atar.yaml',
      'LICENSE',
      'README.md'
    ].filter(f => fs.existsSync(path.join(this.cwd, f)));

    if (includeFiles.length === 0) {
      throw new Error('No files to package');
    }

    // Create tar using tar command
    const tarFile = `${name}.tar`;
    const cmd = `cd "${this.cwd}" && tar -cf "${tarFile}" ${includeFiles.join(' ')}`;

    try {
      execSync(cmd, { stdio: 'inherit' });
      console.log(`✅ Created ${tarFile}\n`);
    } catch (err) {
      throw new Error(`Failed to create tar: ${err.message}`);
    }
  }

  // atar get - Show atar.yaml details
  async get(pkgName) {
    if (!pkgName) {
      const yaml = this.loadYaml();
      if (!yaml) {
        throw new Error('No atar.yaml found');
      }
      this.displayYaml(yaml);
      return;
    }

    // Load from package directory (if exists)
    const pkgYamlPath = path.join(
      process.env.AC_PATH || '/path/to/AC',
      'library/elib',
      pkgName,
      'atar.yaml'
    );

    if (fs.existsSync(pkgYamlPath)) {
      const yaml = this.parseYaml(fs.readFileSync(pkgYamlPath, 'utf8'));
      this.displayYaml(yaml);
    } else {
      throw new Error(`Package not found: ${pkgName}`);
    }
  }

  // atar ready - Auto-generate atar.yaml
  async ready() {
    console.log('\n🔧 Getting ready...\n');

    // Check if atar.yaml exists
    let yaml = this.loadYaml();
    if (!yaml) {
      yaml = {};
    }

    // Detect license
    let license = 'NO LICENSE';
    if (fs.existsSync(this.licensePath)) {
      const licenseContent = fs.readFileSync(this.licensePath, 'utf8');
      if (licenseContent.includes('MIT')) license = 'MIT';
      else if (licenseContent.includes('Apache')) license = 'Apache 2.0';
      else if (licenseContent.includes('GPL')) license = 'GPL 3.0';
    }

    // Get git info
    const gitInfo = this.getGitInfo();

    // Create/update atar.yaml with detected values
    yaml.license = license;
    if (!yaml.name) yaml.name = this.getDirectoryName();
    if (!yaml.version) yaml.version = '0.1.0';
    if (!yaml.author && gitInfo.author) yaml.author = gitInfo.author;
    if (!yaml.repository && gitInfo.remote) yaml.repository = gitInfo.remote;
    if (!yaml.main) yaml.main = 'lib.ac';

    // Save atar.yaml
    this.saveYaml(yaml);
    console.log('✅ atar.yaml created\n');
    console.log(`📖 Metadata:`);
    console.log(`  Name: ${yaml.name}`);
    console.log(`  Version: ${yaml.version}`);
    console.log(`  License: ${license}`);
    if (yaml.author) console.log(`  Author: ${yaml.author}`);
    if (yaml.repository) console.log(`  Repository: ${yaml.repository}`);

    // Create tar
    console.log('');
    await this.package(yaml.name);

    console.log(`📝 Ready to publish: atar publish ${yaml.name}.tar\n`);
  }

  // atar publish - Upload to GitHub
  async publish(tarFile) {
    if (!tarFile) {
      const yaml = this.loadYaml();
      if (!yaml || !yaml.name) {
        throw new Error('No atar.yaml found');
      }
      tarFile = `${yaml.name}.tar`;
    }

    if (!fs.existsSync(path.join(this.cwd, tarFile))) {
      throw new Error(`${tarFile} not found. Run: atar package`);
    }

    console.log(`\n📤 Publishing ${tarFile} to GitHub...\n`);
    console.log('⚠️  Manual steps:');
    console.log(`  1. git tag v1.0.0`);
    console.log(`  2. git push origin v1.0.0`);
    console.log(`  3. gh release create v1.0.0 ${tarFile}`);
    console.log('');
  }

  // atar install - Download from GitHub
  async install(pkg) {
    if (!pkg) {
      throw new Error('Usage: atar install user/package');
    }

    console.log(`\n📥 Installing ${pkg} from GitHub...\n`);
    console.log('⚠️  Manual steps:');
    console.log(`  1. cd /path/to/AC/library/elib`);
    console.log(`  2. gh release download -R ${pkg} --pattern '*.tar'`);
    console.log(`  3. tar -xf *.tar`);
    console.log(`  4. rm *.tar`);
    console.log('');
  }

  // Helper methods
  loadYaml() {
    if (!fs.existsSync(this.atarYamlPath)) {
      return null;
    }
    return this.parseYaml(fs.readFileSync(this.atarYamlPath, 'utf8'));
  }

  parseYaml(content) {
    const yaml = {};
    content.split('\n').forEach(line => {
      line = line.trim();
      if (line && !line.startsWith('#')) {
        const [key, ...value] = line.split(':');
        yaml[key.trim()] = value.join(':').trim();
      }
    });
    return yaml;
  }

  saveYaml(yaml) {
    const lines = [];
    lines.push('# atar.yaml - AC Package Metadata');
    lines.push('');
    lines.push(`name: ${yaml.name}`);
    lines.push(`version: ${yaml.version}`);
    if (yaml.author) lines.push(`author: ${yaml.author}`);
    lines.push(`description: ${yaml.description || 'AC library'}`);
    lines.push(`license: ${yaml.license || 'NO LICENSE'}`);
    if (yaml.repository) lines.push(`repository: ${yaml.repository}`);
    if (yaml.homepage) lines.push(`homepage: ${yaml.homepage}`);
    lines.push(`main: ${yaml.main || 'lib.ac'}`);

    fs.writeFileSync(this.atarYamlPath, lines.join('\n') + '\n');
  }

  displayYaml(yaml) {
    console.log('\n📦 Package Info\n');
    console.log(`Name: ${yaml.name}`);
    console.log(`Version: ${yaml.version}`);
    if (yaml.author) console.log(`Author: ${yaml.author}`);
    console.log(`Description: ${yaml.description || '(none)'}`);
    console.log(`License: ${yaml.license || 'NO LICENSE'}`);
    if (yaml.repository) console.log(`Repository: ${yaml.repository}`);
    if (yaml.homepage) console.log(`Homepage: ${yaml.homepage}`);
    console.log(`Main: ${yaml.main || 'lib.ac'}`);
    console.log('');
  }

  findTarFile() {
    const files = fs.readdirSync(this.cwd);
    return files.some(f => f.endsWith('.tar'));
  }

  isGitRepo() {
    try {
      execSync('git rev-parse --git-dir', { cwd: this.cwd, stdio: 'pipe' });
      return true;
    } catch {
      return false;
    }
  }

  hasGitRemote() {
    try {
      execSync('git config --get remote.origin.url', { cwd: this.cwd, stdio: 'pipe' });
      return true;
    } catch {
      return false;
    }
  }

  getGitInfo() {
    const info = { author: null, remote: null };
    try {
      info.author = execSync('git config user.name', { cwd: this.cwd, stdio: 'pipe' })
        .toString().trim();
    } catch {}
    try {
      info.remote = execSync('git config --get remote.origin.url', { cwd: this.cwd, stdio: 'pipe' })
        .toString().trim();
    } catch {}
    return info;
  }

  getDirectoryName() {
    return path.basename(this.cwd);
  }
}

module.exports = Atar;
