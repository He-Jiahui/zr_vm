use std::collections::{HashMap, HashSet, VecDeque};

pub const MOD: i64 = 1_000_000_007;

pub fn scale_from_tier(tier: &str) -> Option<i64> {
    match tier {
        "smoke" => Some(1),
        "core" => Some(4),
        "stress" => Some(16),
        "profile" => Some(1),
        _ => None,
    }
}

fn mod_reduce(value: i64) -> i64 {
    let reduced = value % MOD;
    if reduced < 0 {
        reduced + MOD
    } else {
        reduced
    }
}

pub fn numeric_loops(scale: i64) -> i64 {
    let outer_limit = 24 * scale;
    let inner_limit = 3000 * scale;
    let mut value = 17_i64;
    let mut checksum = 0_i64;

    for outer in 0..outer_limit {
        for inner in 0..inner_limit {
            value = (value * 1103 + 97 + outer + inner) % 65521;
            if value % 7 == 0 {
                checksum += value / 7;
            } else if value % 5 == 0 {
                checksum += value % 97;
            } else {
                checksum += value % 31;
            }
            checksum = mod_reduce(checksum);
        }
        checksum = mod_reduce(checksum + outer * 17 + value % 13);
    }

    checksum
}

trait DispatchWorker {
    fn step(&mut self, delta: i64) -> i64;
    fn read(&self) -> i64;
}

struct MultiplyWorker {
    state: i64,
}

impl DispatchWorker for MultiplyWorker {
    fn step(&mut self, delta: i64) -> i64 {
        self.state = (self.state * 13 + delta + 7) % 10007;
        self.state
    }

    fn read(&self) -> i64 {
        self.state
    }
}

struct ScaleWorker {
    state: i64,
}

impl DispatchWorker for ScaleWorker {
    fn step(&mut self, delta: i64) -> i64 {
        self.state = (self.state * 17 + delta * 3 + 11) % 10009;
        self.state
    }

    fn read(&self) -> i64 {
        self.state
    }
}

struct XorWorker {
    state: i64,
}

impl DispatchWorker for XorWorker {
    fn step(&mut self, delta: i64) -> i64 {
        self.state = ((self.state ^ (delta + 31)) + delta * 5 + 19) % 10037;
        self.state
    }

    fn read(&self) -> i64 {
        self.state
    }
}

struct DriftWorker {
    state: i64,
}

impl DispatchWorker for DriftWorker {
    fn step(&mut self, delta: i64) -> i64 {
        self.state = (self.state + delta * delta + 23) % 10039;
        self.state
    }

    fn read(&self) -> i64 {
        self.state
    }
}

fn dispatch(worker: &mut dyn DispatchWorker, delta: i64) -> i64 {
    worker.step(delta)
}

pub fn dispatch_loops(scale: i64) -> i64 {
    let outer_limit = 120 * scale;
    let inner_limit = 320 * scale;
    let mut workers: Vec<Box<dyn DispatchWorker>> = vec![
        Box::new(MultiplyWorker { state: 17 }),
        Box::new(ScaleWorker { state: 29 }),
        Box::new(XorWorker { state: 43 }),
        Box::new(DriftWorker { state: 61 }),
    ];
    let mut checksum = 0_i64;

    for outer in 0..outer_limit {
        for inner in 0..inner_limit {
            let index = ((outer + inner) & 3) as usize;
            let delta = outer * 7 + inner * 11 + index as i64;
            let value = dispatch(workers[index].as_mut(), delta);
            checksum = mod_reduce(checksum + value * (index as i64 + 1) + delta % 29);
        }

        checksum = mod_reduce(checksum + workers[(outer & 3) as usize].read() * (outer + 1));
    }

    checksum
}

fn container_label(value: i64) -> String {
    if value % 2 == 0 {
        "even".to_string()
    } else if value > 128 {
        "odd_hi".to_string()
    } else {
        "odd_lo".to_string()
    }
}

pub fn container_pipeline(scale: i64) -> i64 {
    let total = 1024 * scale;
    let mut queue: VecDeque<(String, i64)> = VecDeque::new();
    let mut seen: HashSet<(i64, String)> = HashSet::new();
    let mut buckets: HashMap<String, Vec<i64>> = HashMap::new();
    let mut seed = 41_i64;

    for index in 0..total {
        seed = (seed * 29 + 17 + index) % 257;
        queue.push_back((container_label(seed), seed * scale + (index % 13)));
    }

    while let Some((label, value)) = queue.pop_front() {
        seen.insert((value, label));
    }

    for (value, label) in seen {
        buckets.entry(label).or_default().push(value);
    }

    let odd_lo_sum: i64 = buckets.get("odd_lo").map_or(0, |items| items.iter().sum());
    let odd_hi_sum: i64 = buckets.get("odd_hi").map_or(0, |items| items.iter().sum());
    let even_sum: i64 = buckets.get("even").map_or(0, |items| items.iter().sum());
    mod_reduce(even_sum * 100000 + odd_hi_sum * 100 + odd_lo_sum + buckets.values().map(|items| items.len() as i64).sum::<i64>())
}

fn insertion_sort(values: &mut [i64]) {
    for index in 1..values.len() {
        let key = values[index];
        let mut cursor = index;
        while cursor > 0 && values[cursor - 1] > key {
            values[cursor] = values[cursor - 1];
            cursor -= 1;
        }
        values[cursor] = key;
    }
}

fn build_sort_pattern(pattern: usize, length: usize) -> Vec<i64> {
    let mut values = Vec::with_capacity(length);
    let mut seed = 97_i64;

    if pattern == 0 {
        for index in 0..length {
            seed = (seed * 1103515245 + 12345 + index as i64) % 2147483647;
            values.push(seed % 100000);
        }
        return values;
    }

    if pattern == 1 {
        for index in 0..length {
            values.push((length - index) as i64);
        }
        return values;
    }

    if pattern == 2 {
        for index in 0..length {
            values.push(((index as i64) * 17 + 3) % (length as i64 / 8 + 5));
        }
        return values;
    }

    for index in 0..length {
        values.push(index as i64 * 3 + (index as i64 % 7));
    }
    for index in (0..length).step_by(7) {
        let swap_index = (index * 13 + 5) % length;
        values.swap(index, swap_index);
    }
    values
}

pub fn sort_array(scale: i64) -> i64 {
    let length = (16 * scale) as usize;
    let mut step = length / 7;
    let mut checksum = 0_i64;
    if step < 1 {
        step = 1;
    }

    for pattern in 0..4 {
        let mut values = build_sort_pattern(pattern, length);
        let mut cursor = 0;
        let mut subtotal = 0_i64;
        insertion_sort(&mut values);
        while cursor < length {
            subtotal = mod_reduce(subtotal + values[cursor] * (cursor as i64 + 1));
            cursor += step;
        }
        subtotal = mod_reduce(
            subtotal
                + values[0] * 3
                + values[length / 2] * 5
                + values[length - 1] * 7
                + pattern as i64 * 11,
        );
        checksum = mod_reduce(checksum * 131 + subtotal);
    }

    checksum
}

pub fn prime_trial_division(scale: i64) -> i64 {
    let limit = 5000 * scale;
    let mut checksum = 0_i64;
    let mut count = 0_i64;

    for candidate in 2..=limit {
        let mut divisor = 2_i64;
        let mut is_prime = true;
        while divisor * divisor <= candidate {
            if candidate % divisor == 0 {
                is_prime = false;
                break;
            }
            divisor += 1;
        }
        if is_prime {
            count += 1;
            checksum = mod_reduce(checksum + candidate * ((count % 97) + 1));
        }
    }

    checksum
}

pub fn matrix_add_2d(scale: i64) -> i64 {
    let rows = (24 * scale) as usize;
    let cols = (32 * scale) as usize;
    let cells = rows * cols;
    let mut lhs = vec![0_i64; cells];
    let mut rhs = vec![0_i64; cells];
    let mut dst = vec![0_i64; cells];
    let mut scratch = vec![0_i64; cells];
    let mut checksum = 0_i64;

    for index in 0..cells {
        lhs[index] = (index as i64 * 13 + 7) % 997;
        rhs[index] = (index as i64 * 17 + 11) % 991;
    }

    for row in 0..rows {
        let mut row_sum = 0_i64;
        for col in 0..cols {
            let index = row * cols + col;
            dst[index] = lhs[index] + rhs[index] + ((row as i64 + col as i64) % 7);
            scratch[index] = dst[index] - lhs[index] / 3 + (rhs[index] % 11);
            row_sum += scratch[index] * (col as i64 + 1);
        }
        checksum = mod_reduce(checksum + row_sum * (row as i64 + 1));
    }

    for (index, value) in scratch.iter().enumerate() {
        checksum = mod_reduce(checksum + value * ((index as i64 % 17) + 1));
    }

    checksum
}

pub fn string_build(scale: i64) -> i64 {
    let fragments = ["al", "be", "cy", "do", "ex", "fu"];
    let mut counts: HashMap<String, i64> = HashMap::new();
    let mut keys: Vec<String> = Vec::new();
    let mut assembled = String::new();
    let mut assembled_score = 0_i64;
    let mut checksum = 0_i64;
    let mut seed = 17_i64;
    let iterations = 180 * scale;

    for index in 0..iterations {
        seed = (seed * 73 + 19 + index) % 997;
        let token_id = (seed + index) % 23;
        let token = format!(
            "{}-{}{}",
            fragments[(seed as usize) % fragments.len()],
            fragments[(token_id as usize) % fragments.len()],
            fragments[((token_id + 2) as usize) % fragments.len()]
        );
        let token_score = (seed % 211) + token_id * 17 + index;
        assembled.push_str(&token);
        assembled_score = mod_reduce(assembled_score * 41 + token_score);
        if index % 4 == 0 {
            assembled.push('|');
            assembled_score = mod_reduce(assembled_score + 3);
        } else {
            assembled.push(':');
            assembled_score = mod_reduce(assembled_score + 7);
        }

        if index % 9 == 8 {
            if !counts.contains_key(&assembled) {
                counts.insert(assembled.clone(), 0);
                keys.push(assembled.clone());
            }
            let next_value = mod_reduce(counts[&assembled] + assembled_score + index + 1);
            counts.insert(assembled.clone(), next_value);
            checksum = mod_reduce(checksum + next_value + (seed % 97));
            assembled = token;
            assembled_score = mod_reduce(token_score);
        }
    }

    if !assembled.is_empty() {
        if !counts.contains_key(&assembled) {
            counts.insert(assembled.clone(), 0);
            keys.push(assembled.clone());
        }
        let next_value = mod_reduce(counts[&assembled] + assembled_score + iterations);
        counts.insert(assembled.clone(), next_value);
    }

    for (index, key) in keys.iter().enumerate() {
        checksum = mod_reduce(checksum + counts[key] * (index as i64 + 1));
    }

    checksum
}

pub fn map_object_access(scale: i64) -> i64 {
    let labels = ["aa", "bb", "cc", "dd"];
    let mut buckets: HashMap<String, i64> = HashMap::new();
    let mut checksum = 0_i64;
    let mut left = 3_i64;
    let mut right = 7_i64;
    let mut hits = 0_i64;

    for outer in 0..(64 * scale) {
        for inner in 0..32_i64 {
            left = (left * 31 + outer + inner + hits) % 10007;
            right = (right + left + inner * 3 + 5) % 10009;
            hits += 1;
            let label = labels[((outer + inner) % labels.len() as i64) as usize];
            let key = format!("{label}_slot");
            let next_value = mod_reduce(*buckets.get(&key).unwrap_or(&0) + left + right + hits);
            buckets.insert(key, next_value);
            checksum = mod_reduce(checksum + next_value + left + hits);
        }
    }

    mod_reduce(
        checksum
            + buckets["aa_slot"]
            + buckets["bb_slot"]
            + buckets["cc_slot"]
            + buckets["dd_slot"],
    )
}
