export type TestResult = {
  name: string;
  passed: boolean;
  error?: any;
};

export class TestSuite {
  results: TestResult[] = [];

  async test(name: string, fn: () => void | Promise<void>) {
    try {
      await fn();
      this.results.push({ name, passed: true });
    } catch (error) {
      this.results.push({ name, passed: false, error });
    }
  }

  report() {
    const passed = this.results.filter(r => r.passed).length;
    const failed = this.results.filter(r => !r.passed).length;
    
    console.log(`\n--- Test Report ---`);
    this.results.forEach(r => {
      console.log(`${r.passed ? '✅' : '❌'} ${r.name}`);
      if (r.error) {
        console.error(`   Error: ${r.error.message || r.error}`);
        if (r.error.stack) console.error(r.error.stack);
      }
    });
    console.log(`-------------------`);
    console.log(`Passed: ${passed}, Failed: ${failed}`);
    
    if (failed > 0) {
      throw new Error(`Testing failed with ${failed} failure(s).`);
    }
  }
}

export function assert(condition: boolean, message: string) {
  if (!condition) {
    throw new Error(`Assertion Failed: ${message}`);
  }
}

export function assertEquals(actual: any, expected: any, message?: string) {
  if (actual !== expected) {
    throw new Error(`Assertion Failed: ${message || ''} Expected ${expected}, but got ${actual}`);
  }
}
