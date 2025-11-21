-- phpMyAdmin SQL Dump
-- version 5.2.1
-- https://www.phpmyadmin.net/
--
-- Host: 127.0.0.1
-- Generation Time: Nov 20, 2025 at 09:44 AM
-- Server version: 10.4.32-MariaDB
-- PHP Version: 8.2.12

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `db_garmenttracking`
--

-- --------------------------------------------------------

--
-- Table structure for table `garment`
--

CREATE TABLE `garment` (
  `id_garment` int(11) NOT NULL,
  `rfid_card` varchar(36) NOT NULL,
  `item` varchar(30) NOT NULL,
  `buyer` varchar(30) NOT NULL,
  `style` varchar(30) NOT NULL,
  `wo` varchar(30) NOT NULL,
  `isDone` varchar(30) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT current_timestamp(),
  `updated` timestamp NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `garment`
--

INSERT INTO `garment` (`id_garment`, `rfid_card`, `item`, `buyer`, `style`, `wo`, `isDone`, `timestamp`, `updated`) VALUES
(1, '0003830632', '', '', '', '', 'done', '2025-11-18 05:54:46', '2025-11-19 01:38:41'),
(2, '0003817549', '', '', '', '', '', '2025-11-18 05:54:46', '2025-11-19 01:38:41'),
(3, '0005210456', '', '', '', '', '', '2025-11-18 05:54:46', '2025-11-19 01:38:41'),
(4, '0003830632', '', '', '', '', '', '2025-11-18 05:54:46', '2025-11-19 01:38:41');

-- --------------------------------------------------------

--
-- Table structure for table `garment_wo`
--

CREATE TABLE `garment_wo` (
  `style` varchar(30) NOT NULL,
  `item` varchar(30) NOT NULL,
  `buyer` varchar(30) NOT NULL,
  `wo` varchar(30) NOT NULL,
  `qty` int(11) NOT NULL,
  `color` varchar(30) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `rfid_temp`
--

CREATE TABLE `rfid_temp` (
  `id_mac` varchar(20) NOT NULL,
  `tag_id` varchar(20) NOT NULL,
  `style` varchar(30) NOT NULL,
  `item` varchar(30) NOT NULL,
  `buyer` varchar(30) NOT NULL,
  `wo` varchar(20) NOT NULL,
  `nik_op` varchar(30) NOT NULL,
  `nik_qc` varchar(30) NOT NULL,
  `nik_pqc` varchar(30) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `rfid_temp`
--

INSERT INTO `rfid_temp` (`id_mac`, `tag_id`, `style`, `item`, `buyer`, `wo`, `nik_op`, `nik_qc`, `nik_pqc`, `timestamp`) VALUES
('aa', '0', 'style_TEST', 'item_TEST', 'buyer_TEST', 'wo_TEST', '111111', '222222', '333333', '2025-11-19 06:41:01');

-- --------------------------------------------------------

--
-- Table structure for table `tracking_movement`
--

CREATE TABLE `tracking_movement` (
  `id` int(11) NOT NULL,
  `id_garment` int(11) NOT NULL,
  `nik` varchar(50) NOT NULL,
  `last_status` varchar(30) NOT NULL,
  `nama` varchar(30) NOT NULL,
  `bagian` varchar(50) NOT NULL,
  `line` varchar(30) NOT NULL,
  `rfid_card` varchar(36) NOT NULL,
  `style` varchar(30) NOT NULL,
  `item` varchar(30) NOT NULL,
  `buyer` varchar(30) NOT NULL,
  `wo` varchar(30) NOT NULL,
  `rejectCount` int(11) NOT NULL,
  `reworkCount` int(11) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tracking_movement`
--

INSERT INTO `tracking_movement` (`id`, `id_garment`, `nik`, `last_status`, `nama`, `bagian`, `line`, `rfid_card`, `style`, `item`, `buyer`, `wo`, `rejectCount`, `reworkCount`, `timestamp`) VALUES
(9, 0, '111111', 'rework', 'OPERATOR_ROBOTIC', 'OPERATOR', '26', '005210456', 'style_TEST', 'item_TEST', 'buyer_TEST', '', 0, 0, '2025-11-19 07:08:02'),
(10, 0, '', 'good', 'OPERATOR_ROBOTIC', 'OPERATOR', '26', '005210456', 'style_TEST', 'item_TEST', 'buyer_TEST', '', 0, 0, '2025-11-20 04:20:25'),
(12, 0, '111111', 'reject', 'OPERATOR_ROBOTIC', 'OPERATOR', '26', '005210456', 'style_TEST', 'item_TEST', 'buyer_TEST', '', 0, 0, '2025-11-19 07:08:02');

-- --------------------------------------------------------

--
-- Table structure for table `tracking_movement_end`
--

CREATE TABLE `tracking_movement_end` (
  `id` int(11) NOT NULL DEFAULT 0,
  `id_garment` int(11) NOT NULL,
  `nik` varchar(50) NOT NULL,
  `last_status` varchar(30) NOT NULL,
  `nama` varchar(30) NOT NULL,
  `bagian` varchar(50) NOT NULL,
  `line` varchar(30) NOT NULL,
  `rfid_card` varchar(36) NOT NULL,
  `style` varchar(30) NOT NULL,
  `item` varchar(30) NOT NULL,
  `buyer` varchar(30) NOT NULL,
  `wo` varchar(30) NOT NULL,
  `rejectCount` int(11) NOT NULL,
  `reworkCount` int(11) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `user`
--

CREATE TABLE `user` (
  `id_user` int(11) NOT NULL,
  `rfid_user` varchar(30) NOT NULL,
  `telegram` varchar(30) NOT NULL,
  `no_hp` varchar(30) NOT NULL,
  `password` varchar(30) NOT NULL,
  `nama` varchar(50) NOT NULL,
  `nik` varchar(50) NOT NULL,
  `bagian` varchar(30) NOT NULL,
  `line` varchar(30) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT current_timestamp(),
  `updated` timestamp NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `user`
--

INSERT INTO `user` (`id_user`, `rfid_user`, `telegram`, `no_hp`, `password`, `nama`, `nik`, `bagian`, `line`, `timestamp`, `updated`) VALUES
(1, '', '', '', '111111', 'OPERATOR_ROBOTIC', '111111', 'OPERATOR', '26', '2025-11-18 05:51:25', '2025-11-19 01:08:30'),
(2, '', '', '', '222222', 'QC_ROBOTIC', '222222', 'QC', '26', '2025-11-18 05:51:25', '2025-11-19 01:08:33'),
(3, '', '', '', '333333', 'PQC_ROBOTIC', '333333', 'PQC', '26', '2025-11-18 05:51:25', '2025-11-19 01:08:34'),
(4, '', '', '', '444444', 'DRYROOM_ROBOTIC', '444444', 'DRYROOM', '0', '2025-11-18 05:51:25', '2025-11-19 01:08:36'),
(5, '', '', '', '555555', 'FINISHING_ROBOTIC', '555555', 'FINISHING', '0', '2025-11-18 05:51:25', '2025-11-19 01:08:38'),
(6, '', '', '', '666666', 'IE_ROBOTIC', '666666', 'IE', '0', '2025-11-18 05:51:25', '2025-11-19 01:08:40'),
(7, '', '', '', '92200055', 'RIZKI SAPUTRA SEMBIRING', '92200055', 'ROBOTIC', '26', '2025-11-18 05:51:25', '2025-11-19 01:08:42'),
(8, '', '', '', '92300014', 'PRENDI SUPRATMAN', '92300014', 'ROBOTIC', '26', '2025-11-20 03:50:19', '2025-11-20 06:57:34'),
(0, '0006196846', '', '', '', 'FebriMakatita', '92500067', 'ROBOTIC', '69', '2025-11-20 06:54:45', '2025-11-20 06:54:45');

--
-- Indexes for dumped tables
--

--
-- Indexes for table `garment`
--
ALTER TABLE `garment`
  ADD PRIMARY KEY (`id_garment`);

--
-- Indexes for table `garment_wo`
--
ALTER TABLE `garment_wo`
  ADD PRIMARY KEY (`wo`),
  ADD UNIQUE KEY `wo` (`wo`);

--
-- Indexes for table `rfid_temp`
--
ALTER TABLE `rfid_temp`
  ADD PRIMARY KEY (`id_mac`);

--
-- Indexes for table `tracking_movement`
--
ALTER TABLE `tracking_movement`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `user`
--
ALTER TABLE `user`
  ADD PRIMARY KEY (`nik`),
  ADD KEY `nik` (`nik`),
  ADD KEY `nama` (`nama`),
  ADD KEY `bagian` (`bagian`,`line`),
  ADD KEY `line` (`line`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `garment`
--
ALTER TABLE `garment`
  MODIFY `id_garment` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=5;

--
-- AUTO_INCREMENT for table `tracking_movement`
--
ALTER TABLE `tracking_movement`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=13;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
