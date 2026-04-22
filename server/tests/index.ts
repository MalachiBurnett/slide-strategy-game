import { runGameLogicTests } from "./gameLogic.test";
import { runValidationTests } from "./validation.test";
import { runInfrastructureTests } from "./db.test";

export async function runUnitTests() {
  console.log("🧪 Running unit tests...");
  await runGameLogicTests();
  await runValidationTests();
}

export async function runIntegrationTests() {
  console.log("🌐 Running infrastructure tests...");
  await runInfrastructureTests();
}

export async function runAllTests() {
  console.log("🚀 Starting automated system tests...");
  const startTime = Date.now();

  try {
    await runUnitTests();
    // Integration tests should usually come after DB init, 
    // so we'll call them separately in index.ts or handle it here if we assume DB is alive.
    await runIntegrationTests();
    
    const duration = Date.now() - startTime;
    console.log(`✅ All tests passed successfully in ${duration}ms!\n`);
  } catch (error: any) {
    console.error(`\n❌ AUTOMATED TESTS FAILED: ${error.message}`);
    // We might want to exit the process if tests fail, or just warn.
    // Given the user said "automated testing system that runs ... when i startup the server", 
    // it's usually safer to stop boot if core logic is broken.
    if (process.env.NODE_ENV === 'production') {
       console.error("Stopping server due to test failure in production environment.");
       process.exit(1);
    } else {
       console.warn("Continuing server startup despite test failures (development mode).");
    }
  }
}
