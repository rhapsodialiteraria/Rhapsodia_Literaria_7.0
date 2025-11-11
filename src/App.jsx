import React from 'react'
import Navbar from './Components/Navbar/Navbar'
import Hero from './Components/Hero/Hero'
import About from './Components/About/About'
import Services from './Components/Services/Services'
import MyWork from './Components/MyWork/MyWork'
import Contact from './Components/Contact/Contact'
import Footer from './Components/Footer/Footer'
import HostImages from './Components/HostImages/HostImages'
import Day1Events from './Components/Day_1_Events/Day1Events'
import Day2Events from './Components/Day_2_Events/Day2Events'
import Sponsors from './Components/Sponsors/Sponsors'

const App = () => {
  return (
    <div>
      <Navbar />
      <HostImages />
      <Day1Events />
      <Day2Events />
      <Contact />
      <Sponsors />
      <Footer />
    </div>
  )
}

export default App
