import { TestSuite, assert } from "./testUtils";
import { db } from "../db";

export async function runInfrastructureTests() {
  const suite = new TestSuite();

  await suite.test("Database Connectivity", async () => {
    return new Promise((resolve, reject) => {
      db.get("SELECT 1", (err, row) => {
        if (err) reject(err);
        else {
          assert(row !== undefined, "Database should return a row for simple query");
          resolve();
        }
      });
    });
  });

  await suite.test("Table Structure: Users", async () => {
    return new Promise((resolve, reject) => {
      db.get("SELECT name FROM sqlite_master WHERE type='table' AND name='users'", (err, row) => {
        if (err) reject(err);
        else {
          assert(row !== undefined, "Users table should exist");
          resolve();
        }
      });
    });
  });

  await suite.test("Table Structure: Games", async () => {
    return new Promise((resolve, reject) => {
      db.get("SELECT name FROM sqlite_master WHERE type='table' AND name='games'", (err, row) => {
        if (err) reject(err);
        else {
          assert(row !== undefined, "Games table should exist");
          resolve();
        }
      });
    });
  });

  suite.report();
}
